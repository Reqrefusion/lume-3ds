"""One exact RomFS inventory, independent of historical patch baselines."""
from pathlib import Path
import hashlib,json,os,shutil,datetime

def sha(path):
 h=hashlib.sha256()
 with Path(path).open('rb') as f:
  for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
 return h.hexdigest()
def dump(path,obj):
 path=Path(path);path.parent.mkdir(parents=True,exist_ok=True);tmp=path.with_name(path.name+'.tmp');tmp.write_text(json.dumps(obj,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');os.replace(tmp,path)
def files(folder):
 folder=Path(folder)
 for p in sorted(folder.rglob('*')):
  if p.is_symlink():raise ValueError('Symbolic links are not supported: '+str(p))
  if p.is_file():yield p

def inventory(folder):return {p.relative_to(folder).as_posix():{'sha256':sha(p),'bytes':p.stat().st_size} for p in files(folder)}
def source_inventory(root):return inventory(root/'source')
def save(root):
 rom=root/'romfs';data=inventory(rom)
 if not data:raise ValueError('RomFS is empty; run SETUP.cmd first.')
 dump(root/'assets.sha256.json',{'files':data,'count':len(data)})
 dump(root/'source-manifest.json',{'files':source_inventory(root)})
 return data

def verify(root):
 path=root/'assets.sha256.json'
 if not path.is_file():raise ValueError('Asset manifest missing; run SETUP.cmd first.')
 expected=json.loads(path.read_text(encoding='utf-8-sig'))['files'];actual=inventory(root/'romfs')
 if actual!=expected:
  wrong=[n for n in sorted(set(actual)|set(expected)) if actual.get(n)!=expected.get(n)]
  raise ValueError('RomFS changed or missing: '+', '.join(wrong[:12])+'. Run SETUP.cmd to prepare the assets again.')
 return actual

def commit(root,generated):
 """Commit assets and their metadata together, with rollback on a locked file."""
 rom=root/'romfs';changes=[(p,rom/p.relative_to(generated)) for p in files(generated) if not (rom/p.relative_to(generated)).is_file() or sha(p)!=sha(rom/p.relative_to(generated))]
 if not changes:print('Generated content is already current.');return
 expected=inventory(rom)
 for p,dest in changes:expected[dest.relative_to(rom).as_posix()]={'sha256':sha(p),'bytes':p.stat().st_size}
 metadata=root/'build/metadata';metadata.mkdir(parents=True,exist_ok=True)
 dump(metadata/'assets.json',{'files':expected,'count':len(expected)})
 dump(metadata/'sources.json',{'files':source_inventory(root)})
 changes += [(metadata/'assets.json',root/'assets.sha256.json'),(metadata/'sources.json',root/'source-manifest.json')]
 backup=root/'backups'/datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f');staged=[];done=[];originals={}
 try:
  for p,dest in changes:
   dest.parent.mkdir(parents=True,exist_ok=True);tmp=dest.with_name(dest.name+'.stage.tmp');shutil.copyfile(p,tmp);staged.append(tmp)
   if sha(tmp)!=sha(p):raise ValueError('Staged asset verification failed: '+str(p))
   original=None
   if dest.exists():original=backup/dest.relative_to(root);original.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(dest,original)
   originals[dest]=original
  for (p,dest),tmp in zip(changes,staged):os.replace(tmp,dest);done.append(dest)
  for p,dest in changes:
   if sha(p)!=sha(dest):raise ValueError('Written asset verification failed: '+str(dest))
 except BaseException:
  for dest in reversed(done):
   if originals[dest] is None:dest.unlink(missing_ok=True)
   else:shutil.copyfile(originals[dest],dest)
  raise
 finally:
  for tmp in staged:tmp.unlink(missing_ok=True)
 print('Updated assets:',len(changes)-2)
