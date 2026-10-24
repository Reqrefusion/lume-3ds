"""Complete producer graph, empty-tree assembly and transactional publication."""
from pathlib import Path
import os,argparse,shutil,json,sys,datetime,tempfile
from state import sha,write_json,output_files,output_name
import toolchain as cv
from recipe import make
from assemble import normal_and_detail,world,fallback_atlases,digits,still_stereo
from contract import validate

def tree(folder):return {p.relative_to(folder).as_posix():sha(p) for p in output_files(folder)}

def publish(root,produced):
 """Replace the entire asset tree, retaining old assets and metadata on failure."""
 sys.path.insert(0,str(root/'build'));from manifest import save
 rom=root/'romfs'
 if rom.is_dir() and tree(rom)==tree(produced):save(root);print('Game assets are already current.');return
 (root/'build').mkdir(exist_ok=True)
 prepared=Path(tempfile.mkdtemp(prefix='romfs.publish_',dir=root/'build'))
 def ignored(folder,names):return [n for n in names if not output_name(Path(folder).relative_to(produced)/n)]
 shutil.copytree(produced,prepared,ignore=ignored,dirs_exist_ok=True)
 if tree(prepared)!=tree(produced):raise ValueError('Publication copy checksum mismatch.')
 backup=root/'backups'/('exe_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f'));backup.mkdir(parents=True)
 metadata={n:(root/n).read_bytes() if (root/n).is_file() else None for n in ['assets.sha256.json','source-manifest.json']}
 old=rom.exists();installed=False
 try:
  if old:os.replace(rom,backup/'romfs')
  rom.parent.mkdir(parents=True,exist_ok=True);os.replace(prepared,rom);installed=True;save(root)
 except BaseException:
  # Ctrl+C must restore the asset tree and its manifests as one transaction.
  if installed:shutil.rmtree(rom)
  if old and (backup/'romfs').exists():os.replace(backup/'romfs',rom)
  for n,data in metadata.items():
   if data is None:(root/n).unlink(missing_ok=True)
   else:(root/n).write_bytes(data)
  raise
 for n,data in metadata.items():
  if data is not None:(backup/n).write_bytes(data)

def icon(produced,target):
 from PIL import Image
 import struct,numpy as np,lz4.block
 from assemble import frames,movie_frame
 from compress import restore
 from itertools import islice
 bg,overlay=next(islice(frames(produced/'anim_intro.lan'),600,601));mid,fr=bg>>16,bg&65535
 if mid==4:
  data=(produced/('cinema_bg'+str(fr)+'.rgb')).read_bytes()
  if data[:4]==b'LRC1':
   n,w,h,c,m,k,r=struct.unpack_from('<IHHBBBB',data,4);data=restore(lz4.block.decompress(data[16:],uncompressed_size=n),w,c,m)
  image=Image.frombytes('RGB',(320,240),data).convert('RGBA')
 else:image=movie_frame(produced/('media_'+['0175','0740','0940','0945'][mid]+'.lvs'),fr)[0].convert('RGBA')
 image.alpha_composite(overlay);rgb=image.convert('RGB');box=rgb.getbbox()
 if box:rgb=rgb.crop(box)
 rgb.thumbnail((44,44),Image.Resampling.LANCZOS);result=Image.new('RGB',(48,48));result.paste(rgb,((48-rgb.width)//2,(48-rgb.height)//2));target.parent.mkdir(parents=True,exist_ok=True);result.save(target)

def produce(tasks,root,recovered,swf,java,jar,ffmpeg,model):
 root=Path(root);cache=root/'tools/exe-production';cp=os.pathsep.join([str(root/'scripts/java'),str(jar),str(jar.parent/'lib/*')]);classes=tree(root/'scripts/java')
 def motion(stage):
  requests=stage/'requests';make(swf,requests);render=stage/'render';(render/'project/romfs').mkdir(parents=True)
  cv.run_logged([str(java),'-Xmx1024m','-Djava.awt.headless=true','-cp',cp,'SourceGame',str(swf),str(requests)],render)
  shutil.move(str(render/'project/romfs'),str(stage/'romfs'))
  for p in requests.iterdir():
   if p.suffix in ['.sf2','.trf']:shutil.copyfile(p,stage/'romfs'/p.name)
  shutil.rmtree(render)
 motion_dir=tasks.run('game-motion',{'swf':sha(swf),'classes':classes,'recipe':sha(Path(__file__).with_name('recipe.py')),'reader':sha(Path(__file__).with_name('original_swf.py')),'events':sha(Path(__file__).with_name('sound_events.py')),'effect_ids':sorted(__import__('sound_events').supported_effects(root/'source'))},cache/'motion',motion)
 def title(stage):
  from pipeline import java_export
  java_export(stage,swf,java,jar,['ExportTitleMotion','TitleTouch','ExportStudioIntro'])
 title_dir=tasks.run('game-title',{'swf':sha(swf),'classes':classes},cache/'title',title)
 # Source video conversion has its own per-movie checkpoints and raw-payload
 # verification. All inputs originate in this extraction tree.
 from video import produce as videos,MODEL_SHA,MODEL_URL
 model=Path(model).resolve() if model else cv.download(MODEL_URL,cache/'model/depth-small.onnx',MODEL_SHA)
 from production_previews import capture_video,export_all
 with capture_video(root,model):videos(tasks,root,recovered,ffmpeg,model)
 pages=cache/'pages/detail';ui=cache/'ui/romfs';audio=cache/'audio'
 inputs={'close_controls':sha(Path(__file__).with_name('close_controls.py')),'producer':sha(Path(__file__)),'title_stereo':sha(Path(__file__).with_name('title_stereo.py')),'logo_depth':sha(Path(__file__).with_name('logo_depth.py')),'logo_policy':sha(root/'source/game/lume/logo_depth.h'),'motion':tree(motion_dir),'title':tree(title_dir),'pages':tree(pages),'ui':tree(ui),'audio':tree(audio),'videos':{p.relative_to(cache/'video').as_posix():sha(p) for p in (cache/'video').rglob('*.lvs')},'stereo_fields':sha(Path(__file__).with_name('stereo_fields.py')),'assembly':sha(Path(__file__).with_name('assemble.py')),'contract':sha(Path(__file__).with_name('contract.py')),'fast_assets':sha(Path(__file__).with_name('fast_assets.py')),'compress':sha(Path(__file__).with_name('compress.py')),'lan_codec':sha(Path(__file__).with_name('lan_codec.py')),'model':sha(model),'ffmpeg':sha(ffmpeg)}
 def assemble(stage):
  rom=stage/'romfs';normal_and_detail(pages,title_dir/'detail',motion_dir/'romfs',ui,rom)
  shutil.copytree(audio,rom,dirs_exist_ok=True)
  for p in (cache/'video').rglob('*.lvs'):shutil.copyfile(p,rom/p.name)
  fallback_atlases(motion_dir/'romfs',pages,recovered,rom)
  digits(swf,java,jar,stage/'digits-work',rom)
  world(swf,recovered,cache/'video',motion_dir/'romfs',rom,ffmpeg,java,jar)
  still_stereo(rom,model,root)
  from stereo_fields import run as stereo_fields
  field_report=stereo_fields(rom);write_json(stage/'stereo_fields.json',field_report)
  from compress import run as compress_assets
  compression=compress_assets(rom)
  from fast_assets import run as fast_assets
  compression['fast_lossless']=fast_assets(rom)
  from logo_depth import ending as promote_ending
  promote_ending(rom)
  from title_stereo import produce as title_depth
  depth_model=model if model.suffix.lower()=='.onnx' else cv.download(MODEL_URL,cache/'model/depth-small.onnx',MODEL_SHA)
  write_json(stage/'title_stereo.json',title_depth(rom,depth_model))
  compression['after_bytes']=sum(p.stat().st_size for p in output_files(rom))
  compression['asset_groups_bytes']={}
  for p in output_files(rom):
   group='videos' if p.suffix=='.lvs' else 'animations' if p.suffix in ['.lan','.lta'] else 'other'
   compression['asset_groups_bytes'][group]=compression['asset_groups_bytes'].get(group,0)+p.stat().st_size
  write_json(stage/'compression.json',compression)
  report=validate(root,rom);write_json(stage/'validation.json',report)
  if not report['complete_game_assets']:raise ValueError('Complete asset verification failed: '+json.dumps(report,ensure_ascii=False))
  shutil.rmtree(stage/'digits-work')
 result=tasks.run('game-assembled',inputs,cache/'complete',assemble)
 write_json(root/'logs/EXEDEN_BOYUT.json',json.loads((result/'compression.json').read_text(encoding='utf-8')))
 report=validate(root,result/'romfs');write_json(root/'logs/EXEDEN_TAM_DENETIM.json',report)
 if not report['complete_game_assets']:raise ValueError('Generated game content incomplete or corrupt; not published.')
 publish(root,result/'romfs')
 icon(result/'romfs',root/'resources/icon.png')
 write_json(root/'logs/production-status.json',report)
 export_all(tasks,root,result/'romfs',model,ffmpeg)
 print('Complete game assets generated from EXE and verified:',report['total_files'],'files.',flush=True)
 return report
