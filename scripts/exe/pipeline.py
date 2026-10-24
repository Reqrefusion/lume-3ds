"""EXE-only asset production. Work is isolated from installed game assets.

Each producer has a provenance record and content-addressed resume checks.
Readiness is explicit: extraction success is never presented as a complete port.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,struct,subprocess,sys,traceback,inspect
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
sys.path.insert(0,str(HERE));sys.path.insert(0,str(HERE.parent))
import toolchain as cv
from state import Tasks,Lock,sha,write_json
from source_paths import application_source

def recover_source(swf,stage):
 from recover import Recovery,Bits,u16
 b=Path(swf).read_bytes();body=b[8:];bits=Bits(body);rect=bits.rect();p=bits.p//8
 rec=Recovery(stage);rec.scan(body,p+4)
 report=rec.finish({'swf_sha256':sha(swf),'stage_rect_twips':rect,'fps':u16(body,p)/256,'frames':u16(body,p+2)})
 shutil.copyfile(swf,stage/'Lume.swf')
 # Unsupported image tags may remain as raw research data, but a missing movie
 # or an invalid exported sound is never accepted as successful recovery.
 if set(rec.videos)!={175,740,940,945}:raise ValueError('Four original videos not found.')
 if any('Conflicting duplicate' in e.get('error','') for e in rec.errors):raise ValueError('Celisen video paketleri var.')
 write_json(stage/'recovery-summary.json',{'assets':len(rec.assets),'scripts':len(rec.scripts),'warnings':rec.errors,'videos':[{k:v for k,v in a.items() if k!='packets'} for a in report['assets'] if a['kind']=='video']})

def effects(source):
 text=application_source(source,'audio.c').read_text(encoding="utf-8")
 block=text.split('static EffectSample samples[] = {',1)[1].split('};',1)[0]
 return [int(x) for x in re.findall(r'\.id\s*=\s*(\d+)',block)]

def make_audio(recovered,stage,ffmpeg,source):
 from original_swf import Original
 from sound_events import decode as decode_sound
 import numpy as np
 original=Original(Path(recovered)/'Lume.swf')
 manifest=json.loads((recovered/'manifest.json').read_text(encoding="utf-8"));assets={a['id']:a for a in manifest['assets'] if a['kind']=='sound'}
 symbols={v:int(k) for k,v in manifest['symbols'].items()}
 needed={('piano%d.pcm'%i):symbols['piano%d'%i] for i in range(1,9)}
 needed.update({'sfx_%04d.pcm'%i:i for i in effects(source)})
 for name,cid in needed.items():
  if cid not in assets:raise ValueError('Missing original sound: '+str(cid))
  src=recovered/assets[cid]['path']
  if assets[cid]['format']!=2:raise ValueError('Unsupported original audio encoding: '+str(cid))
  decoded=np.frombuffer(decode_sound(original,cid,recovered,ffmpeg),dtype='<i2').reshape(-1,2)
  (stage/name).write_bytes(np.rint(decoded.astype(np.float64).mean(axis=1)).astype('<i2').tobytes())
  n=(stage/name).stat().st_size
  if not n or n%2 or n>4*1024*1024:raise ValueError('Invalid PCM sound size: '+name)
 from sound_events import produce as event_audio
 event_audio(original,recovered,stage,ffmpeg,set(effects(source)))
 music_ids=[int(n) for n in re.search(r'ending\s*\?\s*"(\d+)"\s*:\s*"(\d+)"',application_source(source,'audio.c').read_text(encoding="utf-8")).groups()]
 for cid in music_ids:
  if cid not in assets or assets[cid]['format']!=2:raise ValueError('Original music not found: '+str(cid))
  shutil.copyfile(recovered/assets[cid]['path'],stage/('media_%04d.mp3'%cid))

def java_export(stage,swf,java,jar,classes):
 work=stage/'render';(work/'project/romfs').mkdir(parents=True);(work/'qa').mkdir()
 cp=os.pathsep.join([str(HERE.parent/'java'),str(jar),str(jar.parent/'lib/*')])
 for cls in classes:
  cv.run_logged([str(java),'-Xmx1024m','-Djava.awt.headless=true','-cp',cp,cls,str(swf)],work)
 # Keep rendered assets only; QA PNGs and caches are outside the game output.
 for path in list((work/'project/romfs').rglob('*.tmp')):
  final=path.with_suffix('')
  if not final.is_file():raise ValueError('Incomplete Java output: '+path.name)
  cv.native_valid(final)
  if final.suffix=='.lan':cv.read_lan(final,full=True,dimensions=(320,240) if final.name=='studio_intro.lan' else (800,600))
  path.unlink()
 for path in (work/'project/romfs').rglob('*'):
  if path.is_file():
   cv.native_valid(path)
   if path.suffix=='.lan':cv.read_lan(path,full=True,dimensions=(320,240) if path.name=='studio_intro.lan' else (800,600))
 shutil.copytree(work/'project/romfs',stage/'detail');shutil.rmtree(work)

def audit(root,cache):
 """No silent old-media fallback. These groups are independent completion gates."""
 if (cache/'complete/romfs').is_dir():
  from contract import validate
  report=validate(root,cache/'complete/romfs')
  if report['complete_game_assets']:
   sys.path.insert(0,str(root/'build'))
   from manifest import verify
   try:verify(root)
   except (ValueError,OSError) as e:report['complete_game_assets']=False;report['errors'].append(str(e))
  write_json(root/'logs/production-status.json',report);return report
 groups={}
 def group(name,paths,description):
  missing=[str(p.relative_to(root)) for p in paths if not p.is_file()]
  groups[name]={'ready':not missing,'missing':missing,'description':description}
 rec=cache/'recovered'
 group('extraction',[rec/'Lume.swf',rec/'manifest.json',*[rec/'video'/(c+'.flv') for c in ['0175','0740','0940','0945']]],'Verified SWF, original videos, bitmaps and sound extraction')
 music_ids=re.search(r'ending\s*\?\s*"(\d+)"\s*:\s*"(\d+)"',application_source(root/'source','audio.c').read_text(encoding="utf-8")).groups()
 group('audio',[cache/'audio'/n for n in [*['piano%d.pcm'%i for i in range(1,9)],*['sfx_%04d.pcm'%i for i in effects(root/'source')],*['media_'+n+'.mp3' for n in music_ids]]],'All sounds referenced by current audio.c')
 group('reading-menu-languages',[cache/'ui/romfs/lang/languages.txt',cache/'ui/romfs/lang/tr/font.bin',cache/'ui/romfs/detail/native_menu/art.bin',cache/'ui/romfs/hover.bin'],'Current UI/font/text/translation exporters')
 group('pages-puzzles',[cache/'pages/detail'/(n+'.rgb') for n in ['clue_letter','clue_book','clue_book2','clue_book3_0','clue_book3_10','wires','keys','solar','piano','map','door','cupboard']],'Original-resolution pages and puzzle art')
 group('videos',[cache/'video'/c/('media_'+c+'.lvs') for c in ['0175','0740','0940','0945']],'Original source colour25Hz + model-derived stereo')
 checkpoints={'extraction':[('extract',rec)],'audio':[('audio',cache/'audio')],'reading-menu-languages':[('ui',cache/'ui')],'pages-puzzles':[('pages',cache/'pages')],'videos':[('video-'+c,cache/'video'/c) for c in ['0175','0740','0940','0945']]}
 for name,checks in checkpoints.items():
  errors=[]
  for task,folder in checks:
   try:
    data=json.loads((cache/'state'/(task+'.json')).read_text(encoding="utf-8"))
    outputs=data['outputs']
    if not isinstance(outputs,dict) or not outputs:raise ValueError('Empty asset list')
    changed=[n for n,h in outputs.items() if not (folder/n).is_file() or sha(folder/n)!=h]
    if changed:errors.append({'task':task,'changed':changed})
   except (OSError,ValueError,KeyError,TypeError):errors.append({'task':task,'error':'No valid completion record'})
  groups[name]['checkpoint_errors']=errors
  groups[name]['ready'] &= not errors
 groups['authored-motion-props-scenes']={'ready':(cache/'motion/romfs/anim_intro.lan').is_file() and (cache/'motion/romfs/nav_hall_stairs_after_walk.lan').is_file() and (cache/'title/detail/title_entry.lan').is_file(),'description':'Authored motion, props and animated title producers; final assembly validates their complete output'}
 groups['assembled-and-validated']={'ready':False,'description':'Run EXEDEN_OYUNU_URET.cmd or PRODUCE_AND_BUILD.cmd to finish and publish the complete validated asset tree'}
 ready=all(g['ready'] for g in groups.values())
 report={'input_swf_sha256':cv.SWF_SHA,'groups':groups,'complete_game_assets':ready,'uses_old_buildkit_media':False,'safe_to_replace_installed_romfs':ready}
 write_json(root/'logs/production-status.json',report)
 return report

def main(argv=None):
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--root',default=str(ROOT));p.add_argument('--stage',choices=['extract','audio','ui','pages','video','base','all','audit'],default='base')
 for n in ['exe','java','ffdec','ffmpeg','model']:p.add_argument('--'+n)
 p.add_argument('--prepared-maps');p.add_argument('--force',action='store_true');a=p.parse_args(argv);root=Path(a.root).resolve();cache=root/'tools/exe-production'
 (root/'logs').mkdir(parents=True,exist_ok=True);tasks=Tasks(cache/'state',a.force)
 if a.stage=='audit':
  report=audit(root,cache);print(json.dumps(report,ensure_ascii=False,indent=2));return 0 if report['complete_game_assets'] else 2
 swf=cv.source_swf(root,root/'tools/detail',a)
 base_inputs={'swf':cv.SWF_SHA,'recover':sha(HERE/'recover.py'),'extract_function':hashlib.sha256(inspect.getsource(recover_source).encode()).hexdigest()}
 recovered=tasks.run('extract',base_inputs,cache/'recovered',lambda dest:recover_source(swf,dest))
 if a.stage in ['all','video'] and not a.prepared_maps:
  sys.path.insert(0,str(HERE.parent/'iw3_profile'))
  from iw3_bridge import active as iw3_active
  if a.model:raise ValueError('The iw3 profile does not accept --model ONNX; use scripts/iw3_profile/profile.json')
  a.model=iw3_active(root)
 if a.stage!='extract':
  java,jar,ffmpeg=cv.tools(root/'tools/detail',a,need_ffmpeg=a.stage in ['audio','video','base','all'])
  inputs={**base_inputs,'java':sha(java),'ffdec':sha(jar)}
  if a.stage in ['audio','base','all']:
   tasks.run('audio',{**base_inputs,'ffmpeg':sha(ffmpeg),'audio_source':sha(application_source(root/'source','audio.c')),'sound_events':sha(HERE/'sound_events.py'),'sound_reader':sha(HERE/'original_swf.py'),'producer':hashlib.sha256(inspect.getsource(make_audio).encode()).hexdigest()},cache/'audio',lambda dest:make_audio(recovered,dest,ffmpeg,root/'source'))
  if a.stage in ['ui','base','all']:
   def ui(dest):
    import generate
    # A staging root gives the existing transactional exporter an empty target.
    # It cannot read any previously installed RomFS media from the user's kit.
    shutil.copytree(root/'languages',dest/'languages');(dest/'source').mkdir(parents=True)
    generate.run(argparse.Namespace(root=str(dest),exe=str(swf),java=str(java),ffdec=str(jar),ffmpeg=None,force=True))
    # RomFS is already at the canonical staging location.
    for name in ['tools','source','resources','build','backups','languages']:shutil.rmtree(dest/name,ignore_errors=True)
    for n in ['assets.sha256.json','source-manifest.json']:(dest/n).unlink(missing_ok=True)
   tasks.run('ui',{**inputs,'languages':{p.name:sha(p) for p in (root/'languages').glob('*') if p.is_file()},'language_checker':sha(HERE.parent/'check_languages.py'),'exporters':{p.name:sha(p) for p in (HERE.parent/'java').glob('*.class')},'generate':sha(HERE.parent/'generate.py')},cache/'ui',ui)
  if a.stage in ['pages','base','all']:
   classes=['NativePages','ExportPuzzleArt','ExportPianoEffects','ExportLockProgress','ExportPuzzleFeedback']
   tasks.run('pages',{**inputs,'classes':{p.name:sha(p) for p in (HERE.parent/'java').glob('*.class') if p.stem.split('$')[0] in classes},'producer':hashlib.sha256(inspect.getsource(java_export).encode()).hexdigest()},cache/'pages',lambda dest:java_export(dest,swf,java,jar,classes))
  if a.stage=='all':
   from full import produce as complete
   report=complete(tasks,root,recovered,swf,java,jar,ffmpeg,a.model,prepared=a.prepared_maps)
   return 0
  if a.stage=='video':
   from video import produce,MODEL_URL,MODEL_SHA
   from production_previews import capture_video,ensure_neural,index
   model=Path(a.model).resolve() if a.model else cv.download(MODEL_URL,cache/'model/depth-small.onnx',MODEL_SHA)
   with capture_video(root,model):produce(tasks,root,recovered,ffmpeg,model)
   ensure_neural(root,model,ffmpeg);index(root)
 report=audit(root,cache)
 print('Requested production stage completed. Complete game ready:',report['complete_game_assets'])
 print('Status: logs/production-status.json. Existing game files were not modified.')
 return 2 if a.stage=='all' and not report['complete_game_assets'] else 0

if __name__=='__main__':
 root=ROOT;(root/'logs').mkdir(parents=True,exist_ok=True)
 class Tee:
  def __init__(self,*streams):self.streams=streams
  def write(self,s):
   for f in self.streams:f.write(s);f.flush()
   return len(s)
  def flush(self):
   for f in self.streams:f.flush()
 with Lock(root/'tools/exe-production/production.lock'),(root/'logs/production.log').open('a',encoding='utf-8') as log:
  stdout,stderr=sys.stdout,sys.stderr
  sys.stdout=Tee(stdout,log);sys.stderr=Tee(stderr,log)
  try:result=main()
  except Exception:traceback.print_exc();result=1
  finally:sys.stdout,sys.stderr=stdout,stderr
 raise SystemExit(result)
