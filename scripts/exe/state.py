"""Content-addressed checkpoints; a failed task never becomes a valid cache."""
from pathlib import Path
import hashlib,json,os,time

class Lock:
 """OS releases the lock even after a crash; there is no stale PID deletion."""
 def __init__(self,path):self.path=Path(path)
 def __enter__(self):
  self.path.parent.mkdir(parents=True,exist_ok=True);self.file=self.path.open('a+b')
  if not self.path.stat().st_size:self.file.write(b'0');self.file.flush()
  self.file.seek(0)
  try:
   if os.name=='nt':
    import msvcrt
    msvcrt.locking(self.file.fileno(),msvcrt.LK_NBLCK,1)
   else:
    import fcntl
    fcntl.flock(self.file,fcntl.LOCK_EX|fcntl.LOCK_NB)
  except OSError:
   self.file.close();raise ValueError('Another production process is running in this directory.')
  return self
 def __exit__(self,*args):self.file.close()

def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()

def write_json(p,data):
 p=Path(p);p.parent.mkdir(parents=True,exist_ok=True)
 tmp=p.with_name(p.name+'.tmp');tmp.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');os.replace(tmp,p)

def output_name(name):
 rel=Path(name)
 return not(any(n.startswith('.') or n=='__pycache__' for n in rel.parts) or rel.parts[0] in ['render','digits-work'] or rel.suffix in ['.tmp','.spool','.frames','.part','.compressing'])

def output_files(folder):
 folder=Path(folder)
 for p in folder.rglob('*'):
  rel=p.relative_to(folder)
  if not output_name(rel):continue
  if p.is_file():yield p

class Tasks:
 def __init__(self,folder,force=False):
  self.folder=Path(folder);self.folder.mkdir(parents=True,exist_ok=True);self.force=force;self.events=[]
 def run(self,name,inputs,destination,action):
  import re
  if not re.fullmatch(r'[A-Za-z0-9_-]+',name):raise ValueError('Invalid production name')
  started=time.monotonic();self.last_status='failed'
  try:return self._run(name,inputs,destination,action)
  finally:
   self.events.append({'task':name,'status':self.last_status,'seconds':round(time.monotonic()-started,3)})
   try:write_json(self.folder/'run-summary.json',{'tasks':self.events})
   except OSError as error:print('Sure ozeti yazilamadi:',error,flush=True)
 def _run(self,name,inputs,destination,action):
  import shutil,tempfile
  dest=Path(destination).resolve();record=self.folder/(name+'.json')
  signature=hashlib.sha256(json.dumps(inputs,sort_keys=True).encode()).hexdigest()
  try:saved=json.loads(record.read_text(encoding='utf-8'))
  except (ValueError,OSError):saved={}
  if not isinstance(saved,dict):saved={}
  outputs=saved.get('outputs',{})
  if not isinstance(outputs,dict):outputs={}
  # Invalid checkpoint paths never cause reads outside this task's output.
  if any(not isinstance(n,str) or not n or Path(n).is_absolute() or '..' in Path(n).parts or ':' in n or '\\' in n for n in outputs):outputs={}
  outputs={n:h for n,h in outputs.items() if output_name(n)}
  if not self.force and saved.get('signature')==signature and outputs and all((dest/n).is_file() and sha(dest/n)==h for n,h in outputs.items()) and {p.relative_to(dest).as_posix() for p in output_files(dest)}==set(outputs):
   self.last_status='skipped';print('Verified, skipped:',name,flush=True);return dest
  dest.parent.mkdir(parents=True,exist_ok=True)
  stage=Path(tempfile.mkdtemp(prefix=dest.name+'.building-',dir=dest.parent));backup=None;installed=False
  def remove_generated(path):
   resolved=path.resolve()
   if resolved.parent!=dest.parent or not resolved.name.startswith((dest.name+'.building-',dest.name+'.previous-')):
    raise ValueError('Invalid temporary production directory: '+str(path))
   if resolved.exists():shutil.rmtree(resolved)
  print('Generating:',name,flush=True);start=time.monotonic()
  try:
   action(stage)
   outputs={p.relative_to(stage).as_posix():sha(p) for p in output_files(stage)}
   if not outputs:raise ValueError('Empty production output: '+name)
   if dest.exists():
    backup=Path(tempfile.mkdtemp(prefix=dest.name+'.previous-',dir=dest.parent));backup.rmdir()
    os.replace(dest,backup)
   try:
    os.replace(stage,dest);installed=True
    write_json(record,{'signature':signature,'inputs':inputs,'outputs':outputs,'seconds':round(time.monotonic()-start,2)})
   except BaseException:
    # The checkpoint and destination form one commit. Keep the old record
    # and restore old files if record publication fails or is interrupted.
    if installed:
     os.replace(dest,stage);installed=False
    if backup is not None and backup.exists():os.replace(backup,dest);backup=None
    raise
   self.last_status='built'
   if backup is not None:
    try:remove_generated(backup)
    except OSError as error:print('Previous production copy retained:',error,flush=True)
   return dest
  finally:
   if stage.exists():
    try:remove_generated(stage)
    except OSError as error:print("Temporary production directory retained:",error,flush=True)
