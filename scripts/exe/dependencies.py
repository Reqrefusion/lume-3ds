"""Download pinned Windows wheels directly from PyPI, with SHA256 verification.

Embedded Python needs no system install or get-pip execution. Wheels are data
archives; the same verifier handles pure Python and CPython312/Windows64 wheels.
"""
from pathlib import Path
import sys,os,json,zipfile
sys.path.insert(0,str(Path(__file__).resolve().parent))
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import toolchain as cv
from state import sha,write_json

PINS={'numpy':'1.26.4','pillow':'11.0.0','opencv-python-headless':'4.10.0.84',
 'onnxruntime':'1.20.1','coloredlogs':'15.0.1','humanfriendly':'10.0',
 'pyreadline3':'3.5.4','flatbuffers':'24.3.25','packaging':'24.2','protobuf':'5.29.1',
 'sympy':'1.13.3','mpmath':'1.3.0','lz4':'4.3.3'}

def wheel_score(name):
 if not name.endswith('.whl'):return -1
 tags=name[:-4].rsplit('-',3)[-3:]
 py,abi,platform=tags
 if platform=='any' and ('py3' in py.split('.') or 'py2.py3'==py):return 1
 if platform!='win_amd64':return -1
 if py=='cp312' and abi=='cp312':return 4
 if abi=='abi3' and py.startswith('cp3') and py[3:].isdigit() and 2<=int(py[3:])<=12:return 3
 return -1

def runtime_libraries(root):
 """Install Microsoft's C++ runtime beside portable Python, without an installer."""
 lock=json.loads(Path(__file__).with_name('windows-vclibs.json').read_text(encoding="utf-8"))
 folder=Path(sys.executable).parent
 pending=[n for n,h in lock['files'].items() if not (folder/n).is_file() or sha(folder/n)!=h]
 if not pending:return
 archive=cv.download(lock['url'],Path(root)/'tools/exe-python/downloads/vclibs.appx',lock['sha256'])
 with zipfile.ZipFile(archive) as z:
  names={n.lower():n for n in z.namelist()}
  for name in pending:
   if Path(name).name!=name or not name.endswith('.dll'):raise ValueError('Microsoft runtime file path')
   data=z.read(names[name])
   import hashlib
   if hashlib.sha256(data).hexdigest()!=lock['files'][name]:raise ValueError('Microsoft DLL SHA256 mismatch: '+name)
   temp=folder/(name+'.part');temp.write_bytes(data);os.replace(temp,folder/name)

def install(root):
 if os.name!='nt':raise ValueError('Automatic wheel setup requires Windows x64/Python 3.12; use --host-python on Linux.')
 if sys.version_info[:2]!=(3,12):raise ValueError('Python 3.12 is required.')
 root=Path(root);base=(root/'tools/exe-python').resolve();base.mkdir(parents=True,exist_ok=True)
 records=[]
 locked=json.loads(Path(__file__).with_name('windows-wheels.json').read_text(encoding="utf-8"))
 if {p['name']:p['version'] for p in locked}!=PINS:raise ValueError('Wheel lock does not match package versions.')
 locked={p['name']:p for p in locked}
 for package,version in PINS.items():
  item=locked[package]
  if wheel_score(item['wheel'])<0 or not item['url'].startswith('https://files.pythonhosted.org/'):raise ValueError('Invalid wheel lock: '+package)
  dest=base/package;stamp=dest/'installed.json';wanted={'filename':item['wheel'],'sha256':item['sha256'],'version':version}
  try:saved=json.loads(stamp.read_text(encoding="utf-8"))
  except (ValueError,OSError):saved=None
  valid=isinstance(saved,dict) and saved.get('package')==wanted and all((dest/n).is_file() and sha(dest/n)==h for n,h in saved.get('files',{}).items()) and bool(saved.get('files'))
  if not valid:
   path=cv.download(item['url'],base/'downloads'/item['wheel'],wanted['sha256']);cv.unzip(path,dest)
   write_json(stamp,{'package':wanted,'files':{p.relative_to(dest).as_posix():sha(p) for p in dest.rglob('*') if p.is_file() and p.name!='installed.json'}})
  records.append(wanted)
 paths=[str(base/p) for p in PINS]
 runtime_libraries(root)
 python_folder=Path(sys.executable).resolve().parent
 pth=python_folder/'python312._pth'
 if not pth.is_file():
  if sys.prefix==sys.base_prefix:raise ValueError('Use embedded Python or a project virtual environment; global Python files will not be changed.')
  import site
  pth=Path(site.getsitepackages()[-1])/'lume-production.pth'
  if not pth.exists():pth.write_text('',encoding='utf-8')
 lines=pth.read_text(encoding="utf-8").splitlines();lines=[s for s in lines if not any(tag in s.replace('\\','/').lower() for tag in ['exe-python/','python312-packages/'])]
 for p in paths:
  # Windows may retain the invoked junction path for embedded _pth entries.
  # Absolute installed package paths work through every project junction.
  rel=str(Path(p).resolve()).replace('\\','/')
  if rel not in lines:lines.append(rel)
 pth.write_text('\n'.join(lines)+'\n',encoding='utf-8')
 write_json(base/'resolved.json',records)
 print('Python conversion dependencies are ready. Start a new Python process.')

if __name__=='__main__':install(Path(__file__).resolve().parents[2])
