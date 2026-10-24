"""iw3 producer bridge; final native video codec and game logic are unchanged."""
from pathlib import Path
from contextlib import nullcontext
import hashlib,io,json,os,struct,subprocess,sys
HERE=Path(__file__).resolve().parent
ROOT=None
RUNTIME=None
PROFILE=None

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def active(root):
 global ROOT,RUNTIME,PROFILE
 ROOT=Path(root).resolve()
 from setup_iw3 import ensure,COMMIT
 RUNTIME=ensure(ROOT);PROFILE=HERE/'profile.json'
 config=json.loads(PROFILE.read_text(encoding='utf-8-sig'))
 required={'depth_model':'Any_V2_L','method':'row_flow_v3','fps':25,'synthetic_view':'right','own_size':0}
 if any(config.get(k)!=v for k,v in required.items()):raise ValueError('iw3 profile violates the model/method/25FPS/right-eye/scale contract')
 for k in ['divergence','convergence','foreground_scale','ema_decay']:
  if not isinstance(config[k],(int,float)):raise ValueError('Invalid iw3 setting: '+k)
 descriptor=ROOT/'tools/iw3/active-profile.json'
 data={'engine':'iw3','commit':COMMIT,'profile':config,'bridge_sha256':sha(__file__),'adapter_sha256':sha(HERE/'adapter.py')}
 descriptor.write_text(json.dumps(data,sort_keys=True,indent=2)+'\n',encoding='utf-8')
 import video,assemble,production_previews as pp
 video.produce=produce;assemble.still_stereo=stills
 if 'full' in sys.modules:sys.modules['full'].still_stereo=stills
 pp.capture_video=lambda *a,**kw:nullcontext()
 # These previews are exported by iw3 itself; never rerun the former Small
 # neural estimator over the new Large-model output under an old label.
 pp.ensure_neural=lambda *a,**kw:None
 return str(descriptor)

def run(action,source,dest,expected_frames=None):
 python,repo=RUNTIME
 env=dict(os.environ)
 for k in ['PYTHONHOME','PYTHONPATH','PYTHONUSERBASE','PIP_TARGET']:env.pop(k,None)
 cache_base=ROOT/'tools/iw3/cache' if ROOT else repo.parent/'cache'
 env.update(TORCH_HOME=str(cache_base/'torch'),HF_HOME=str(cache_base/'huggingface'),XDG_CACHE_HOME=str(cache_base/'xdg'))
 from setup_iw3 import execute
 argv=[str(python),str(HERE/'adapter.py'),action,str(PROFILE),str(source),str(dest),'--repo',str(repo)]
 if action=='video':
  if expected_frames is None:raise ValueError('Expected video frame count required')
  argv+=['--expected-frames',str(expected_frames)]
 execute(argv,cwd=repo,env=env)
 doc=json.loads((Path(dest)/'iw3-result.json').read_text())
 # Task output directories are renamed after success. Store relative paths,
 # not the transient .building path emitted by the external adapter.
 base=Path(dest).resolve()
 for k in ['rgb_dir','depth_dir']:doc[k]=Path(doc[k]).resolve().relative_to(base).as_posix()
 (base/'iw3-result.json').write_text(json.dumps(doc,indent=2)+'\n',encoding='utf-8')
 return doc

def frames(folder):
 folder=Path(folder);meta=json.loads((folder/'iw3-result.json').read_text())
 rgb=sorted((folder/meta['rgb_dir']).glob('*.png'));depth=sorted((folder/meta['depth_dir']).glob('*.png'))
 for colour,d in zip(rgb,depth):
  stereo=folder/'sbs'/(colour.stem+'_LRF_Full_SBS.png')
  if not stereo.is_file() or colour.stem!=d.stem:raise ValueError('iw3 RGB/depth/SBS filename mismatch: '+colour.name)
  yield colour,d,stereo

def split(path):
 from PIL import Image
 im=Image.open(path).convert('RGB');w,h=im.size
 if w%2:raise ValueError('iw3 SBS genisligi cift olmali')
 return im.crop((0,0,w//2,h)),im.crop((w//2,0,w,h))

def screen_pair(path,size=(320,240),video=False):
 # Keep exported RGB, 16-bit depth and SBS at source dimensions. Only the
 # console packer downsamples each eye, after iw3 has completed synthesis.
 from PIL import Image
 result=[]
 for eye in split(path):
  if video:
   canvas=Image.new('RGB',(320,240));canvas.paste(eye.resize((320,213),Image.Resampling.LANCZOS),(0,13));eye=canvas
  elif eye.size!=size:eye=eye.resize(size,Image.Resampling.LANCZOS)
  result.append(eye)
 return tuple(result)

def ljv(folder,dest):
 from PIL import Image
 offsets=[];spool=dest.with_suffix('.spool');count=0
 try:
  with spool.open('wb') as out:
   for _,_,sbs in frames(folder):
    left,right=screen_pair(sbs,video=True)
    if left.size!=(320,240) or right.size!=(320,240):raise ValueError('Video SBS her goz icin 320x240 olmali')
    pair=[]
    for im in [left,right]:
     b=io.BytesIO();im.save(b,format='JPEG',quality=100,subsampling=0);pair.append(b.getvalue())
    offsets.append(out.tell());out.write(struct.pack('<II',len(pair[0]),len(pair[1]))+pair[0]+pair[1]);count+=1
  if not count:raise ValueError('Empty iw3 video output')
  size=spool.stat().st_size;header=20+4*(count+1)
  import shutil
  with dest.open('wb') as out,spool.open('rb') as inp:
   out.write(struct.pack('<4sHHIII',b'LJV1',320,240,25,count,0))
   out.write(struct.pack('<%dI'%(count+1),*[header+x for x in offsets+[size]]));shutil.copyfileobj(inp,out)
 finally:spool.unlink(missing_ok=True)
 return count

def preview(folder,dest,video=False):
 from PIL import Image
 import numpy as np,shutil
 dest=Path(dest);shutil.rmtree(dest,ignore_errors=True);dest.mkdir(parents=True,exist_ok=True);step=250 if video else 1
 for i,(rgb,depth,sbs) in enumerate(frames(folder)):
  if i%step:continue
  target=dest/('%06d'%i if video else rgb.stem);target.mkdir(parents=True,exist_ok=True)
  left,right=split(sbs);left.save(target/'L.png');right.save(target/'R.png');shutil.copyfile(sbs,target/'SBS.png')
  shutil.copyfile(depth,target/'depth16.png');d=np.asarray(Image.open(depth),dtype=np.float32)/65535
  np.save(target/'depth.npy',d);Image.fromarray(np.rint(d.clip(0,1)*255).astype(np.uint8)).save(target/'depth_view.png')
  (target/'method.json').write_text(json.dumps({'engine':'iw3','model':'Any_V2_L','frame':i,'fps':25 if video else None,'depth_stage':'actual iw3 normalized export, before foreground mapping/edge correction/row_flow rendering','profile':json.loads(PROFILE.read_text())},indent=2)+'\n',encoding='utf-8')

def source_frames(src,ffmpeg):
 output=subprocess.check_output([str(ffmpeg),'-v','error','-nostdin','-i',str(src),'-vf','fps=25','-an','-progress','pipe:1','-nostats','-f','null','-'],stderr=subprocess.STDOUT,text=True)
 counts=[int(line.split('=',1)[1]) for line in output.splitlines() if line.startswith('frame=')]
 if not counts or max(counts)<1:raise ValueError('Could not verify original 25 FPS video frame count')
 return max(counts)

def prepare_video(src,dest,ffmpeg):
 # iw3 already applies its own FPS filter. Combining that with a second fps
 # filter in --vf misinterprets the FLV 1/1000 clock after conversion to 1/25.
 # Normalize only timing in FFmpeg and retain the original pixel dimensions.
 # A lossless AVI supplies the 1/25 stream clock; iw3 sees source-sized RGB.
 # Screen fitting happens after depth estimation AND stereo synthesis.
 from setup_iw3 import execute
 print('Preparing lossless 25 FPS video for iw3:',Path(src).name,flush=True)
 execute([str(ffmpeg),'-hide_banner','-loglevel','error','-nostdin','-y',
          '-i',str(src),'-map','0:v:0','-an','-sn','-dn',
          '-vf','fps=25,setsar=1,setpts=N/(25*TB)',
          '-c:v','ffv1','-level','3','-pix_fmt','bgr0','-fps_mode','passthrough',
          str(dest)],cwd=Path(dest).parent,env=dict(os.environ))

def produce(tasks,root,recovered,ffmpeg,force_model=None):
 import mpeg_video
 root=Path(root);cache=root/'tools/exe-production';config=mpeg_video.settings(root)
 def plan(stage):
  cuts,details=mpeg_video.cut_plan(Path(recovered)/'Lume.swf',stage)
  (stage/'plan.json').write_text(json.dumps({'cuts':cuts,'source_intervals':details},indent=2)+'\n')
 plan_dir=tasks.run('video-cut-plan',{'swf':sha(Path(recovered)/'Lume.swf'),'recipe':sha(Path(__file__).parent.parent/'exe/recipe.py'),'planner':sha(Path(mpeg_video.__file__))},cache/'video-cut-plan',plan)
 cuts=json.loads((plan_dir/'plan.json').read_text())['cuts']
 from video import IDS
 for cid in IDS:
  src=Path(recovered)/'video'/(cid+'.flv');expected=source_frames(src,ffmpeg)
  def convert(stage,src=src,cid=cid,expected=expected):
   prepared=stage/'input25.avi'
   prepare_video(src,prepared,ffmpeg)
   run('video',prepared,stage,expected_frames=expected)
   count=ljv(stage,stage/(cid+'.ljv'))
   if count!=expected:raise ValueError(f'iw3/source 25 FPS frame count mismatch: {cid}: {count}/{expected}')
  pairs=tasks.run('stereo-'+cid,{'source':sha(src),'iw3_profile':sha(force_model),'bridge':sha(__file__),'fps':25,'source_frames':expected},cache/'stereo'/cid,convert)
  preview(pairs,root/'previews/production/neural'/cid,True)
  def encode(stage,src=src,cid=cid,pairs=pairs):mpeg_video.encode(src,pairs/(cid+'.ljv'),stage/('media_'+cid+'.lvs'),ffmpeg,config,cuts[cid])
  tasks.run('video-'+cid,{'source':sha(src),'stereo':sha(pairs/(cid+'.ljv')),'codec':sha(Path(mpeg_video.__file__)),'flow':sha(Path(__file__).parent.parent/'exe/stereo_resample.py'),'profile':sha(force_model),'settings':config,'cuts':cuts[cid],'ffmpeg':sha(ffmpeg)},cache/'video'/cid,encode)

def still_source(rom,path,project=None):
 # The assembler has already created the normal 3DS RGB alongside its source
 # detail asset. Read the latter directly; upscaling the normal RGB would not
 # recover lost edges. Cinematic plates stay in the motion task, outside RomFS.
 import zlib
 from PIL import Image
 rom=Path(rom);path=Path(path);detail=rom/'detail'/(path.stem+'.dzp')
 if detail.is_file():
  data=detail.read_bytes()
  if len(data)<12 or data[:4]!=b'DZP1':raise ValueError('Invalid detail header: '+str(detail))
  w,h=struct.unpack_from('<II',data,4)
  if not (0<w<=8192 and 0<h<=8192):raise ValueError('Invalid detail size: '+str(detail))
  raw=zlib.decompress(data[12:])
  if len(raw)!=w*h*3:raise ValueError('Detail RGB size mismatch: '+str(detail))
  return Image.frombytes('RGB',(w,h),raw),str(detail)
 if path.stem.startswith('cinema_bg') and project is not None:
  source=Path(project)/'tools/exe-production/motion/romfs/detail'/path.name
  if source.is_file():
   raw=source.read_bytes()
   if len(raw)!=800*600*3:raise ValueError('Original cinema plate size mismatch: '+str(source))
   return Image.frombytes('RGB',(800,600),raw),str(source)
 size=(160,160) if path.stem=='solar_pattern' else (320,240)
 return Image.frombytes('RGB',size,path.read_bytes()),str(path)

def stills(rom,model,preview_root=None):
 import numpy as np,cv2,tempfile,shutil
 from PIL import Image
 rom=Path(rom);work=rom.parent/'iw3-stills';inp=work/'input';inp.mkdir(parents=True,exist_ok=True)
 paths=list(rom.glob('clue_*.rgb'))+list(rom.glob('world_pantry_*.rgb'))+[rom/(n+'.rgb') for n in ['wires','piano','solar','keys','cupboard','map','door','solar_pattern']]+list(rom.glob('cinema_bg[0-9]*.rgb'))
 sizes={};sources={};project=ROOT or preview_root
 for p in paths:
  if p.stem.endswith('_R'):continue
  sizes[p.stem]=(160,160) if p.stem=='solar_pattern' else (320,240)
  image,source=still_source(rom,p,project);image.save(inp/(p.stem+'.png'))
  sources[p.stem]={'source':os.path.relpath(source,work),'source_size':list(image.size),'console_size':list(sizes[p.stem]),'detail_available':image.size!=sizes[p.stem]}
 (work/'source-images.json').write_text(json.dumps(sources,indent=2)+'\n',encoding='utf-8')
 result=work/'converted';run('images',inp,result)
 if preview_root:preview(result,Path(preview_root)/'previews/production/stills')
 dest=rom/'stereo';dest.mkdir(exist_ok=True)
 for colour,_,sbs in frames(result):
  name=colour.stem;left,right=screen_pair(sbs,sizes[name]);a=np.asarray(left);b=np.asarray(right)
  if name in ['wires','keys','door','cupboard']:
   flow=cv2.calcOpticalFlowFarneback(cv2.cvtColor(b,cv2.COLOR_RGB2GRAY),cv2.cvtColor(a,cv2.COLOR_RGB2GRAY),None,.5,3,19,4,5,1.1,0)[:,:,0]
   flow=np.clip(flow,-31.75,31.75);flow[:13]=0;flow[226:]=0
   (dest/(name+'.depth')).write_bytes(struct.pack('<4sHH',b'PDP1',320,240)+np.rint(flow*4).clip(-128,127).astype(np.int8).tobytes())
  if name.startswith('cinema_bg'):(rom/(name+'_R.rgb')).write_bytes(b.tobytes())
  else:
   target=name.removeprefix('clue_').removeprefix('world_');(dest/(target+'_L.rgb')).write_bytes(a.tobytes());(dest/(target+'_R.rgb')).write_bytes(b.tobytes())
 # Every RGB/depth/SBS intermediate remains in the content-addressed complete
 # task beside romfs, and is not shipped to the console.
 (work/'profile.json').write_bytes(Path(model).read_bytes())
