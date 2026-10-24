"""Automatic, separate inspection artifacts; never include these in RomFS.

Observe real neural results during new conversions. Older caches can supply
stored eye pairs and disparity, but missing depth needs one sequential replay.
"""
from contextlib import contextmanager
from pathlib import Path
import html,json,os,shutil,struct,tempfile,zlib
import numpy as np
from PIL import Image
from state import sha,write_json
from stereo_preview import save_pair,lvs_info,lvs_frame,ljv_frame,trace
from video import IDS


def settings(root):
 path=Path(root)/'previews/settings.json'
 defaults={'video_sample_seconds':10,'cinema_sample_seconds':2,'capture_neural_depth':True,'recover_cached_depth':True,'export_source_pages':True}
 if path.is_file():
  supplied=json.loads(path.read_text(encoding='utf-8-sig'))
  defaults.update({k:v for k,v in supplied.items() if k in defaults})
 else:write_json(path,defaults)
 for key in ['video_sample_seconds','cinema_sample_seconds']:
  value=defaults[key]
  if isinstance(value,bool) or not isinstance(value,(int,float)) or not np.isfinite(value) or value<=0:raise ValueError('Invalid preview interval: '+key)
 for key in ['capture_neural_depth','recover_cached_depth','export_source_pages']:
  if not isinstance(defaults[key],bool):raise ValueError('Preview option must be true/false: '+key)
 return defaults


def neural_inputs(root,src,model,every):
 return {'source':sha(src),'model':sha(model),'algorithm':sha(Path(__file__).with_name('stereo_depth.py'))+sha(Path(__file__).resolve().parents[2]/'source/engine/tools/depth_estimator.py'),'fps':15,'sample_seconds':every}


def ready(folder,inputs):
 try:
  doc=json.loads((folder/'complete.json').read_text(encoding='utf-8'))
  return doc['inputs']==inputs and bool(doc['files']) and all((folder/n).is_file() and sha(folder/n)==h for n,h in doc['files'].items())
 except (OSError,ValueError,KeyError,TypeError):return False


def commit(stage,dest,inputs):
 write_json(stage/'complete.json',{'inputs':inputs,'files':{p.relative_to(stage).as_posix():sha(p) for p in stage.rglob('*') if p.is_file()},'depth_kind':'actual normalized and temporally filtered neural output, before JPEG / flow / quarter-pixel packing'})
 backup=dest.with_name(dest.name+'.previous');shutil.rmtree(backup,ignore_errors=True)
 if dest.exists():os.replace(dest,backup)
 try:os.replace(stage,dest)
 except BaseException:
  if backup.exists():os.replace(backup,dest)
  raise
 shutil.rmtree(backup,ignore_errors=True)


class Capture:
 def __init__(self,stereo,folder,every,fps):
  self.stereo=stereo;self.folder=folder;self.step=max(1,round(every*fps));self.fps=fps;self.frame=0
 @property
 def previous(self):return self.stereo.previous
 @previous.setter
 def previous(self,value):self.stereo.previous=value
 def pair(self,rgb,temporal=False):
  left,right,depth=self.stereo.pair(rgb,temporal)
  if self.frame%self.step==0:
   folder=self.folder/('%06d'%self.frame);save_pair(folder,left,right)
   np.save(folder/'depth.npy',depth)
   Image.fromarray(np.rint(depth.clip(0,1)*65535).astype(np.uint16)).save(folder/'depth16.png')
   Image.fromarray(np.rint(depth.clip(0,1)*255).astype(np.uint8)).save(folder/'depth_view.png')
   write_json(folder/'method.json',{'frame':self.frame,'fps':self.fps,'seconds':self.frame/self.fps,'stage':'actual production neural pair, before JPEG; depth is not final disparity'})
  self.frame+=1
  return left,right,depth


@contextmanager
def capture_video(root,model):
 # The producer imports stereo_depth.video when called. A scoped observer
 # keeps the pixel algorithm and existing video cache signatures unchanged.
 # The production lock excludes another writer; restoration happens on error.
 import stereo_depth
 root=Path(root);config=settings(root);original=stereo_depth.video
 def observed(stereo,src,dest,fps):
  if not config['capture_neural_depth']:return original(stereo,src,dest,fps)
  target=root/'previews/production/neural'/Path(src).stem;target.parent.mkdir(parents=True,exist_ok=True)
  inputs=neural_inputs(root,src,model,config['video_sample_seconds'])
  if ready(target,inputs):return original(stereo,src,dest,fps)
  stage=Path(tempfile.mkdtemp(prefix=target.name+'.building-',dir=target.parent))
  try:
   result=original(Capture(stereo,stage,config['video_sample_seconds'],fps),src,dest,fps)
   commit(stage,target,inputs);return result
  except Exception:shutil.rmtree(stage,ignore_errors=True);raise
 stereo_depth.video=observed
 try:yield
 finally:stereo_depth.video=original


def ensure_neural(root,model,ffmpeg):
 root=Path(root);config=settings(root)
 if not config['capture_neural_depth'] or not config['recover_cached_depth']:return
 for cid in IDS:
  src=root/'tools/exe-production/recovered/video'/(cid+'.flv');target=root/'previews/production/neural'/cid
  inputs=neural_inputs(root,src,model,config['video_sample_seconds'])
  if ready(target,inputs):continue
  print('Generating missing intermediate depth images from the original video once:',cid,flush=True)
  target.parent.mkdir(parents=True,exist_ok=True);stage=Path(tempfile.mkdtemp(prefix=cid+'.building-',dir=target.parent))
  try:
   trace(root,cid,stage,ffmpeg,model,config['video_sample_seconds'],5)
   prepared=stage/cid/'neural';commit(prepared,target,inputs)
  finally:shutil.rmtree(stage,ignore_errors=True)


def rgb_file(path):
 from compress import restore
 import lz4.block
 data=path.read_bytes()
 if data[:4]==b'LRC1':
  n,w,h,c,mode,kind,reserved=struct.unpack_from('<IHHBBBB',data,4)
  if c!=3 or kind!=1 or reserved:raise ValueError('RGB header: '+str(path))
  raw=restore(lz4.block.decompress(data[16:],uncompressed_size=n),w,c,mode)
 else:
  w,h=(160,160) if path.stem=='solar_pattern' else (320,240);raw=data
 return Image.frombytes('RGB',(w,h),raw)


def source_page(path):
 from fast_assets import dzp_decode
 return Image.frombytes('RGB',(800,600),dzp_decode(path.read_bytes()))


def snapshot(root,rom,out,config):
 from assemble import frames
 root,rom,out=Path(root),Path(rom),Path(out);out.mkdir(parents=True,exist_ok=True)
 for cid in IDS:
  movie=rom/('media_'+cid+'.lvs');magic,w,h,fps,n,key=lvs_info(movie)
  step=max(1,round(config['video_sample_seconds']*fps));indices=sorted(set(range(0,n,step))|{n-1})
  pairs=root/'tools/exe-production/stereo'/cid/(cid+'.ljv')
  for index in indices:
   folder=out/'videos'/cid/('%06d'%index);left,right,disp=lvs_frame(movie,index);save_pair(folder/'final',left,right)
   np.save(folder/'final/disparity_quarter_pixel.npy',disp)
   Image.fromarray((disp.astype(np.int16)+128).astype(np.uint8)).save(folder/'final/disparity_view.png')
   if pairs.is_file():
    initial,a,b=ljv_frame(pairs,index/fps);save_pair(folder/'intermediate_ljv',a,b)
   write_json(folder/'method.json',{'frame':index,'fps':fps,'seconds':index/fps,'disparity_units':'signed quarter pixels; visualization adds 128, neutral=128'})
 if config['export_source_pages']:
  names=set(['wires','piano','solar','keys','cupboard','map','door','solar_pattern'])
  names.update(p.stem for pattern in ['world_*.rgb','clue_*.rgb'] for p in rom.glob(pattern))
  for name in sorted(names):
   p=rom/(name+'.rgb')
   if not p.is_file():continue
   folder=out/'pages'/name;folder.mkdir(parents=True,exist_ok=True);im=rgb_file(p);im.save(folder/'overview.png')
   displacement=rom/'stereo'/(name+'.depth')
   if displacement.is_file():
    payload=displacement.read_bytes();disp=np.frombuffer(payload[8:],np.int8).reshape(240,320)
    left=np.asarray(im);yy,xx=np.mgrid[:240,:320];q=(xx*4+disp).clip(0,1276);sx=q>>2;f=(q&3)[:,:,None]
    right=((left[yy,sx].astype(np.uint16)*(4-f)+left[yy,np.minimum(sx+1,319)].astype(np.uint16)*f+2)>>2).astype(np.uint8)
    save_pair(folder,left,right);np.save(folder/'disparity_quarter_pixel.npy',disp)
    Image.fromarray((disp.astype(np.int16)+128).astype(np.uint8)).save(folder/'disparity_view.png')
    write_json(folder/'stereo_method.json',{'method':'complete source board reprojected with the runtime quarter-pixel map','state':'source page baseline; in-game keys/wires/digits are composited before the same warp','touch_screen':'unchanged left/source coordinates'})
   high=rom/'detail'/(name+'.dzp')
   if high.is_file():source_page(high).save(folder/'source.png')
   if name in ['wires','piano','solar','keys','cupboard','map','door']:im.crop((0,13,32,45)).resize((128,128),Image.Resampling.NEAREST).save(folder/'close_pixels.png')
 for name in ['map_ink','digits_door','digits_cupboard']:
  p=rom/'detail'/(name+'.lan')
  if not p.is_file():continue
  folder=out/'layers'/name;folder.mkdir(parents=True,exist_ok=True)
  for index,(bg,layer) in enumerate(frames(p)):
   box=layer.getbbox()
   if box:layer.crop(box).save(folder/('%03d.png'%index))
 for name in ['anim_intro','anim_ending']:
  folder=out/'cinema'/name;folder.mkdir(parents=True,exist_ok=True)
  selected={0,81,82,88,89} if name=='anim_intro' else {0}
  data=(rom/(name+'.lan')).read_bytes();total=struct.unpack_from('<I',data,16)[0];selected.add(total-1)
  selected.update(range(0,total,max(1,round(config['cinema_sample_seconds']*25))))
  for index,(bg,overlay) in enumerate(frames(rom/(name+'.lan'))):
   if index not in selected:continue
   mid,fr=bg>>16,bg&65535
   if mid==4:left=rgb_file(rom/('cinema_bg'+str(fr)+'.rgb'))
   else:left=lvs_frame(rom/('media_'+IDS[mid]+'.lvs'),fr)[0]
   base=folder/('%06d'%index);base.mkdir(exist_ok=True);left.save(base/'background.png');overlay.save(base/'overlay.png')
   composed=left.convert('RGBA');composed.alpha_composite(overlay);composed.convert('RGB').save(base/'composite.png')
   write_json(base/'frame.json',{'clip_frame':index,'seconds':index/25,'source_root_frame':1752+index if name=='anim_intro' else 2538+index,'background_movie':mid,'background_frame':fr,'includes_native_lower_screen_skip':False})
 write_json(out/'settings.json',config)


def index(root):
 base=Path(root)/'previews/production';base.mkdir(parents=True,exist_ok=True);items=[]
 # Keep the gallery light: individual raw/overlay/maps remain linked beside
 # each overview, instead of embedding every intermediate into the HTML.
 patterns=['snapshots/**/SBS.png','snapshots/**/overview.png','snapshots/**/composite.png','snapshots/layers/**/*.png','neural/**/SBS.png','stills/**/SBS.png']
 for pattern in patterns:
  for p in sorted(base.glob(pattern)):
   rel=p.relative_to(base).as_posix();folder=p.parent
   links=' '.join('<a href="'+html.escape(q.relative_to(base).as_posix())+'">'+html.escape(q.name)+'</a>' for q in sorted(folder.iterdir()) if q.is_file() and q!=p)
   items.append('<figure><figcaption>'+html.escape(str(p.parent.relative_to(base)))+'</figcaption><a href="'+html.escape(rel)+'"><img loading="lazy" src="'+html.escape(rel)+'"></a><p>'+links+'</p></figure>')
 reports=[]
 for cid in IDS:
  source=Path(root)/'tools/exe-production/video'/cid/'quality.json'
  if not source.is_file():continue
  report=json.loads(source.read_text(encoding='utf-8'));target=base/'video_reports'/cid/'quality.json';target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
  if 'lossless_colour_frames' not in report:continue
  reports.append('<li><a href="'+target.relative_to(base).as_posix()+'">'+cid+'</a>: '+str(report['frames'])+' frames; '+str(report['bytes'])+' bytes; lossless color '+str(report['lossless_colour_frames'])+' frames; derinlik '+str(report['disparity_payload_bytes'])+' bytes; below quality target '+str(report.get('groups_below_target',0))+' groups.</li>')
 plan=Path(root)/'tools/exe-production/video-cut-plan/plan.json'
 if plan.is_file():
  target=base/'video_reports/cut_plan.json';target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(plan,target);reports.append('<li><a href="video_reports/cut_plan.json">Clip bounds relative to original frames</a></li>')
 compression=Path(root)/'tools/exe-production/complete/compression.json'
 if compression.is_file():
  target=base/'video_reports/lan_compression.json';target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(compression,target);reports.append('<li><a href="video_reports/lan_compression.json">LAN: per-file sizes before/after and frame modes</a></li>')
 report_html='<h2>Video compression report</h2><p>Quality is compared with FFmpeg reference decoding. This report does not measure console smoothness or ARM decoding.</p><ul>'+''.join(reports)+'</ul>' if reports else ''
 (base/'index.html').write_text('<!doctype html><meta charset="utf-8"><title>Lume production previews</title><style>body{background:#222;color:#eee;font:16px sans-serif}a{color:#9cf}figure{margin:24px 0}img{max-width:100%;max-height:600px}p{max-width:1000px}</style><h1>EXE production previews</h1><p>Stored separately from game files. Neural depth and final disparity are different stages. PNG images and numerical NPY arrays are linked below. Sampling settings: ../settings.json.</p>'+report_html+''.join(items),encoding='utf-8')
 print('Intermediate previews:',base/'index.html',flush=True)


def export_all(tasks,root,rom,model,ffmpeg):
 root=Path(root);config=settings(root);ensure_neural(root,model,ffmpeg)
 inputs={'assembled_record':sha(root/'tools/exe-production/state/game-assembled.json'),'code':sha(Path(__file__)),'preview_decoder':sha(Path(__file__).with_name('stereo_preview.py')),'mpeg_decoder':sha(Path(__file__).with_name('mpeg_video.py')),'lan_decoder':sha(Path(__file__).with_name('lan_codec.py')),'settings':config}
 tasks.run('inspection-previews',inputs,root/'previews/production/snapshots',lambda stage:snapshot(root,rom,stage,config))
 index(root)
