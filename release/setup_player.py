"""Player setup: original EXE + maps -> external data beside a fixed app."""
from pathlib import Path
import argparse,contextlib,hashlib,json,os,sys
ROOT=Path(__file__).resolve().parents[1]
EXPECTED_EXE='ba7a947b9a501a1723508e388764113e27b85db7bc14f52b613473373cad0089'

def digest(path):
 with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def find_original(root=ROOT,explicit=None):
 if explicit:
  path=Path(explicit).expanduser().resolve()
  if not path.is_file() or digest(path)!=EXPECTED_EXE:raise ValueError('The selected file is not the supported original Lume.exe.')
  return path
 roots=[Path(os.environ.get('ProgramFiles(x86)',r'C:\Program Files (x86)'))/'Steam/steamapps/common/Lume',Path(r'C:\Program Files (x86)\Steam\steamapps\common\Lume'),Path(root)]
 checked=set()
 for folder in roots:
  path=folder/'Lume.exe'
  if path in checked:continue
  checked.add(path)
  if path.is_file() and digest(path)==EXPECTED_EXE:return path.resolve()
 raise FileNotFoundError('Supported Lume.exe not found. Select your installed game or pass --exe PATH.')

class ProgressLog:
 def __init__(self,console,log,verbose=False):self.console,self.log,self.verbose=console,log,verbose;self.pending='';self.last={}
 def write(self,text):
  self.log.write(text);self.log.flush()
  if self.verbose:self.console.write(text);self.console.flush();return len(text)
  self.pending+=text
  while '\n' in self.pending:
   line,self.pending=self.pending.split('\n',1);line=line.strip()
   if line.startswith(('Generating: ','Verified, skipped: ')):
    task=line.split(': ',1)[1];names={'extract':'Reading your original game','audio':'Preparing music and sounds','ui':'Preparing text and menus','pages':'Preparing game images','game-motion':'Preparing animations','game-title':'Preparing the title screen','prepared-video-cut-plan':'Reading video timing','game-assembled':'Combining original content and ready 3D maps'}
    message=names.get(task)
    if message:self.console.write(message+(' (already prepared)' if line.startswith('Verified') else '')+'...\n')
   elif line.startswith('MPEG '):
    parts=line.split()
    if len(parts)>=5:
     percent=int(parts[2])*100//int(parts[4]);bucket=percent//10
     if self.last.get(parts[1])!=bucket:self.last[parts[1]]=bucket;self.console.write('Preparing video '+str(['0175','0740','0940','0945'].index(parts[1])+1)+'/4: '+str(percent)+'%\n')
   elif line.startswith(('Original game verified:','Prepared geometry verified:','Packaging your 3DS game','Copying the fixed app','READY:','Copy the 3ds','SETUP FAILED:')):self.console.write(line+'\n')
   self.console.flush()
  return len(text)
 def flush(self):self.log.flush();self.console.flush()

def preparation_arguments(root,original,languages_only=False):
 if languages_only and not (Path(root)/'assets.sha256.json').is_file():raise ValueError('Prepare the game once with SETUP.cmd before updating languages.')
 return ['--root',str(root),'--stage','ui' if languages_only else 'all','--exe',str(original),'--prepared-maps',str(Path(root)/'maps')]

def run(args):
 original=find_original(ROOT,args.exe);output=Path(args.output).resolve();game=output/'3ds/lume.3dsx'
 sys.path.insert(0,str(ROOT/'release'));from export_maps import prepared_files
 files=prepared_files(ROOT)
 runtime=ROOT/'player/lume.3dsx';metadata=json.loads((ROOT/'player/runtime.json').read_text(encoding='utf-8'))
 if digest(runtime)!=metadata['sha256'] or metadata.get('original_game_content_embedded') is not False or metadata.get('storage')!='external_sd':raise ValueError('Runtime integrity check failed.')
 for name,value in metadata['source'].items():
  path=ROOT/'source'/name
  if not path.is_file() or digest(path)!=value['sha256']:raise ValueError('Game source changed; a developer must rebuild the media-free runtime before player setup.')
 print('Original game verified:',original,flush=True)
 print('Prepared geometry verified:',len(files)-2,'files. No depth model or 3DS compiler will run.',flush=True)
 sys.path.insert(0,str(ROOT/'scripts/exe'));sys.path.insert(0,str(ROOT/'scripts'));sys.path.insert(0,str(ROOT/'build'))
 from state import Lock
 import pipeline
 class NoInference:
  def find_spec(self,fullname,path=None,target=None):
   if fullname.split('.')[0] in {'onnxruntime','torch','stereo_depth','stereo_resample','iw3_bridge'}:raise RuntimeError('Player setup must not run a depth model or stereo matching: '+fullname)
 sys.meta_path.insert(0,NoInference())
 # mpeg_video's prepared path must not import the unused optical-flow source.
 arguments=preparation_arguments(ROOT,original,args.languages_only)
 if args.languages_only:
  from manifest import verify
  verify(ROOT)
 for name in ['java','ffdec','ffmpeg']:
  value=getattr(args,name)
  if value:arguments+=['--'+name,value]
 with Lock(ROOT/'tools/exe-production/production.lock'):
  result=pipeline.main(arguments)
  if result:raise RuntimeError('Original media preparation failed.')
  from manifest import verify,commit
  if args.languages_only:commit(ROOT,ROOT/'tools/exe-production/ui/romfs')
  expected=verify(ROOT)
  print('Copying the fixed app and converted game content...',flush=True)
  from player_content import publish
  report={'original_sha256':digest(original),'geometry_files':len(files)-2,'depth_inference':False,'stereo_matching':False,'native_compiler_run':False,'game_binary_generated':False,'private_original_media':True,'hardware_tested':False,'languages_only':args.languages_only}
  publish(runtime,ROOT/'romfs',output,expected,report)
 print('READY:',game,flush=True)
 print('Copy the 3ds folder to your SD card and launch Lume. Preserve existing files in /3ds/lume/.',flush=True)
 return game

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--exe');parser.add_argument('--languages-only',action='store_true');parser.add_argument('--verbose',action='store_true');parser.add_argument('--output',default=str(ROOT/'out'))
 for name in ['java','ffdec','ffmpeg']:parser.add_argument('--'+name)
 args=parser.parse_args();logs=ROOT/'logs';logs.mkdir(exist_ok=True)
 with (logs/'player-setup.log').open('a',encoding='utf-8') as log,contextlib.redirect_stdout(ProgressLog(sys.stdout,log,args.verbose)),contextlib.redirect_stderr(ProgressLog(sys.stderr,log,args.verbose)):
  try:run(args)
  except Exception as error:
   import traceback
   traceback.print_exc();print('SETUP FAILED:',error,flush=True);print('See logs/player-setup.log for details.',file=sys.__stderr__,flush=True);return 1
 return 0
if __name__=='__main__':raise SystemExit(main())