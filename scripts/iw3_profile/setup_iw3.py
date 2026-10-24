"""Private iw3 installation; never installs into the game's embedded Python."""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys,tempfile,urllib.request,zipfile
from model_checkpoint import ensure_checkpoint
COMMIT='0d3d92c8f4396da564c44524b6390ee5c76b9668'
SOURCE_SHA='31ce2da698faa10b8d3ea50afc40e0f9d15f0aa2aaa5aae92c702fbb82daf130'
CHECKED_SOURCE={'iw3/utils.py': 'f20dcff1604c61a55fcfae28650358b284cc9a33cbc5e8b63baa1db7daae7e51', 'iw3/export_config.py': 'd5d58895337ca5fa88b7cad1d49a418303d865c1502a13768b9cd9b36fa7b9e6', 'requirements-torch.txt': '9d53e1e889fa433f39bfd8125be7530035597c408851c325a5f787a043960168', 'requirements.txt': 'dfa931e3983166d4709768c7a85fcd8c8288e19f331de81d5bb7f27c47fb6ab0'}
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]

def download(url,path,expected=None):
 path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
 if path.is_file() and (expected is None or hashlib.sha256(path.read_bytes()).hexdigest()==expected):return path
 print('Downloading:',path.name,flush=True);req=urllib.request.Request(url,headers={'User-Agent':'Lume-local-iw3-producer'})
 temp=path.with_suffix(path.suffix+'.part')
 with urllib.request.urlopen(req,timeout=90) as src,temp.open('wb') as dst:shutil.copyfileobj(src,dst,1024*1024)
 if expected and hashlib.sha256(temp.read_bytes()).hexdigest()!=expected:temp.unlink();raise ValueError('Download SHA256 mismatch: '+path.name)
 os.replace(temp,path);return path

def extract(path,dest):
 dest=Path(dest);dest.mkdir(parents=True,exist_ok=True)
 with zipfile.ZipFile(path) as z:
  for info in z.infolist():
   if not (dest/info.filename).resolve().is_relative_to(dest.resolve()):raise ValueError('Invalid ZIP path')
  if z.testzip():raise ValueError('Corrupt ZIP')
  z.extractall(dest)

def execute(argv,cwd,env):
 process=subprocess.Popen(list(map(str,argv)),cwd=cwd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding='utf-8',errors='replace')
 try:
  for line in process.stdout:print(line,end='',flush=True)
  rc=process.wait()
  if rc:raise subprocess.CalledProcessError(rc,argv)
 finally:
  if process.poll() is None:process.terminate();process.wait()
  process.stdout.close()

def ensure(root=ROOT):
 root=Path(root).resolve();base=root/'tools/iw3';python=base/'python/python.exe';repo=base/'nunif';record=base/'ready.json'
 expected={'commit':COMMIT,'source_sha256':SOURCE_SHA,'python':'3.12.10'}
 if record.is_file() and python.is_file() and json.loads(record.read_text())==expected:
  if not all((repo/n).is_file() and hashlib.sha256((repo/n).read_bytes()).hexdigest()==h for n,h in CHECKED_SOURCE.items()):raise ValueError('iw3 sources differ from the pinned revision; not overwritten automatically.')
  ensure_checkpoint(repo)
  return python,repo
 if os.name!='nt':raise ValueError('Automatic iw3 setup requires Windows.')
 base.mkdir(parents=True,exist_ok=True);downloads=base/'downloads'
 package=download(f'https://codeload.github.com/nagadomi/nunif/zip/{COMMIT}',downloads/'nunif-source.zip',SOURCE_SHA)
 if (repo/'iw3/utils.py').is_file() and not all((repo/n).is_file() and hashlib.sha256((repo/n).read_bytes()).hexdigest()==h for n,h in CHECKED_SOURCE.items()):raise ValueError('Different sources found in tools/iw3/nunif; not overwritten automatically.')
 if not (repo/'iw3/utils.py').is_file():
  stage=Path(tempfile.mkdtemp(prefix='source-',dir=base))
  try:extract(package,stage);os.replace(stage/('nunif-'+COMMIT),repo)
  finally:shutil.rmtree(stage,ignore_errors=True)
 archive=download('https://www.python.org/ftp/python/3.12.10/python-3.12.10-embed-amd64.zip',downloads/'python.zip')
 if not python.is_file():extract(archive,python.parent)
 pth=python.parent/'python312._pth';pth.write_text('python312.zip\n.\n../nunif\nimport site\n',encoding='utf-8')
 env=dict(os.environ)
 for k in ['PYTHONHOME','PYTHONPATH','PYTHONUSERBASE','PIP_TARGET']:env.pop(k,None)
 env.update(PIP_CACHE_DIR=str(base/'cache/pip'),TORCH_HOME=str(base/'cache/torch'),HF_HOME=str(base/'cache/huggingface'),XDG_CACHE_HOME=str(base/'cache/xdg'),PIP_DISABLE_PIP_VERSION_CHECK='1')
 getpip=download('https://bootstrap.pypa.io/get-pip.py',downloads/'get-pip.py')
 def run(args):execute([str(python),*map(str,args)],cwd=repo,env=env)
 if subprocess.run([str(python),'-m','pip','--version'],env=env,stdout=subprocess.DEVNULL).returncode:run([getpip])
 run(['-m','pip','install','-r',repo/'requirements-torch.txt'])
 run(['-m','pip','install','-r',repo/'requirements.txt'])
 run(['-m','pip','install','opencv-python-headless'])
 # Mark the dependency installation separately so a failed checkpoint download
 # never triggers another PyTorch installation. The same check runs above when
 # ready.json already exists. Other row-flow models remain upstream-managed.
 run(['-m','iw3','--help'])
 record.write_text(json.dumps(expected,indent=2)+'\n',encoding='utf-8')
 ensure_checkpoint(repo)
 return python,repo
if __name__=='__main__':ensure()
