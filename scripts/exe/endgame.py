"""Regenerate ending overlays, lock art and feedback locally; no video encoding."""
from pathlib import Path
import argparse,json,os,sys,shutil,tempfile
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE));sys.path.insert(0,str(HERE.parent))
import toolchain as cv
from state import Lock,Tasks,sha,write_json

def prepare(root,swf,java,jar,stage):
 from assemble import lan,frames,dzp,resize
 from PIL import Image
 from recipe import make
 import generate
 cp=os.pathsep.join([str(HERE.parent/'java'),str(jar),str(jar.parent/'lib/*')])
 def run(cls,args,folder):
  (folder/'project/romfs').mkdir(parents=True,exist_ok=True);(folder/'qa').mkdir(exist_ok=True)
  cv.run_logged([str(java),'-Xmx1024m','-Djava.awt.headless=true','-cp',cp,cls,str(swf),*map(str,args)],folder)
 # Keep locale generation isolated until all new visual assets are ready.
 ui=stage/'ui';(ui/'source').mkdir(parents=True)
 shutil.copytree(root/'languages',ui/'languages')
 generate.run(argparse.Namespace(root=str(ui),exe=str(swf),java=str(java),ffdec=str(jar),ffmpeg=None,force=True))
 rom=stage/'romfs';shutil.copytree(ui/'romfs',rom)
 pages=stage/'pages';run('NativePages',['--locks'],pages);run('ExportPuzzleFeedback',['--results'],pages)
 raw=pages/'project/romfs';(rom/'detail').mkdir(exist_ok=True)
 selected={'digits_door','digits_cupboard','ui_wires_correct','ui_wires_incorrect','ui_map_correct','ui_solar_win','ui_result_timer','ui_power_fade'}
 for name in ['door','cupboard']:
  data=(raw/(name+'.rgb')).read_bytes()
  if len(data)!=1440000:raise ValueError('Lock source dimensions: '+name)
  im=Image.frombytes('RGB',(800,600),data);(rom/(name+'.rgb')).write_bytes(resize(im,(320,240)).tobytes());dzp(rom/'detail'/(name+'.dzp'),data)
 for name in sorted(selected):
  p=raw/(name+'.lan');cv.read_lan(p,full=True)
  shutil.copyfile(p,rom/'detail'/p.name);lan(rom/p.name,frames(p))
 requests=stage/'requests';make(swf,requests)
 motion=stage/'motion';run('SourceGame',[requests,'ending'],motion)
 ending=motion/'project/romfs/anim_ending.lan'
 last=None;count=0
 for _,im in frames(ending):last=im;count+=1
 if count!=1908 or last is None:raise ValueError('Ending source frame count')
 # The endpoint is the opaque black stage + original Lume logo. Transparent
 # letterbox rows are black; no film decode is needed for this static screen.
 alpha=last.getchannel('A');hist=alpha.crop((0,14,320,226)).histogram()
 if sum(hist[:255]):raise ValueError('Ending endpoint unexpectedly requires a movie background')
 final=Image.new('RGB',(320,240),'black');final.paste(last,(0,0),alpha)
 for eye in ['L','R']:(rom/('ending_'+eye+'.rgb')).write_bytes(final.tobytes())
 # Native logo/UI occupies the screen plane, not the old text's disparity.
 import struct,lz4.block
 field=b'SDF1'+struct.pack('<HH',320,240)+lz4.block.compress(bytes(76800),store_size=False)
 (rom/'ending_L.rgb.sdf').write_bytes(field)
 shutil.copyfile(ending,rom/'anim_ending.lan');shutil.copyfile(requests/'anim_ending.sf2',rom/'anim_ending.sf2')
 from compress import run as compress
 report=compress(rom)
 write_json(stage/'generation.json',{'ending_frames':count,'timed_dialogues':7,'video_conversion':False,'depth_model_used':False,'compression':report})
 for folder in [ui,pages,motion,requests]:shutil.rmtree(folder)

def prepare_runtime(root,generated,dest):
 """Re-use the global fast-media budget without resetting its old ledger.

 Only the newly generated files are repacked here. Other movie/animation
 files remain in place; their verified growth still reserves budget.
 """
 import fast_assets
 target=root/'romfs';shutil.copytree(generated,dest)
 try:previous=json.loads((target/'media_runtime_budget.json').read_text(encoding='utf-8')).get('files',{})
 except (OSError,ValueError,AttributeError):previous={}
 retained={}
 if isinstance(previous,dict):
  for name,entry in previous.items():
   rel=Path(name)
   if rel.is_absolute() or '..' in rel.parts or ':' in name or (dest/rel).exists():continue
   file=target/rel
   if isinstance(entry,dict) and file.is_file() and entry.get('sha256')==sha(file):retained[name]=entry
 reserved=sum(max(0,int(e.get('growth',0))) for e in retained.values())
 report,_=fast_assets.build(dest,max(0,fast_assets.DEFAULT_BUDGET-reserved))
 added=json.loads((dest/'media_runtime_budget.json').read_text(encoding='utf-8'))['files']
 retained.update(added)
 write_json(dest/'media_runtime_budget.json',{'format':1,'files':retained})
 report['unchanged_media_growth_reserved']=reserved
 return report

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',default=str(HERE.parents[1]))
 for n in ['exe','java','ffdec']:p.add_argument('--'+n)
 a=p.parse_args();root=Path(a.root).resolve();cache=root/'tools/exe-production'
 if not (root/'romfs').is_dir():raise ValueError('Generate the complete game assets first.')
 with Lock(cache/'production.lock'):
  swf=cv.source_swf(root,root/'tools/detail',a);java,jar,_=cv.tools(root/'tools/detail',a,need_ffmpeg=False)
  inputs={'swf':sha(swf),'script':sha(Path(__file__)),
   'java_sources':{x.name:sha(x) for x in (HERE.parent/'java').glob('*.class')},
   'languages':{x.name:sha(x) for x in (root/'languages').glob('*.json')},
   'producers':{n:sha(HERE/n) for n in ['recipe.py','assemble.py','compress.py','lan_codec.py']},'ui':sha(HERE.parent/'generate.py')}
  output=Tasks(cache/'state').run('ending-and-puzzle-feedback',inputs,cache/'ending-feedback',lambda stage:prepare(root,swf,java,jar,stage))
  sys.path.insert(0,str(root/'build'));from manifest import commit
  report=json.loads((output/'generation.json').read_text(encoding='utf-8'))
  with tempfile.TemporaryDirectory(prefix='ending-runtime-',dir=cache) as temp:
   ready=Path(temp)/'romfs';report['fast_runtime']=prepare_runtime(root,output/'romfs',ready)
   commit(root,ready)
  write_json(root/'logs/endgame.json',report)
 print('Ending, translations, locks and result animations are ready. Videos unchanged. Run BUILD.cmd.')
if __name__=='__main__':
 try:main()
 except Exception as exc:print('ERROR:',exc);raise SystemExit(1)
