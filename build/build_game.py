"""Use native devkitARM tools directly: no Bash, drive mounts or make clean."""
from pathlib import Path
import os,json,subprocess,hashlib,struct,time,datetime
from concurrent.futures import ThreadPoolExecutor
from manifest import sha,dump,verify,source_inventory
from romfs_compact import compact_3dsx
from package_cache import signature as package_signature,reusable
from source_graph import source_units,object_path,compile_signature
ARCH=['-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft']

def sdk_tools(root):
 base=root/'tools/sdk/opt/devkitpro';arm=base/'devkitARM';ext='.exe' if os.name=='nt' else ''
 result={'base':base,'arm':arm,'gcc':arm/'bin'/('arm-none-eabi-gcc'+ext),'ctru':base/'libctru','port':base/'portlibs/3ds','pack':base/'tools/bin'/('3dsxtool'+ext),'smdh':base/'tools/bin'/('smdhtool'+ext)}
 for name in ['gcc','pack','smdh']:
  if not result[name].is_file():raise ValueError('Missing SDK tool: '+str(result[name])+'. Pass your existing SDK directory with --sdk.')
 libraries={'ctru':[result['ctru']],**{name:[result['port']] for name in ['mpg123','jpeg','lz4','z']},**{name:[result['port'],result['ctru']] for name in ['citro2d','citro3d']}}
 for name,folders in libraries.items():
  candidates=[folder/'lib'/('lib'+name+'.a') for folder in folders]
  if not any(path.is_file() for path in candidates):raise ValueError('Missing SDK library lib'+name+'.a; searched: '+', '.join(map(str,candidates)))
 return result

def run(cmd,env,cwd):
 print('Tool:',Path(str(cmd[0])).name,flush=True)
 p=subprocess.run([str(x).replace('\\','/') for x in cmd],env=env,cwd=cwd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace')
 if p.stdout:print(p.stdout.rstrip(),flush=True)
 if p.returncode:raise RuntimeError('Command failed (code %s): %s'%(p.returncode,json.dumps([str(x) for x in cmd],ensure_ascii=False)))
 return p.stdout

def validate_3dsx(path):
 with path.open('rb') as f:h=f.read(44)
 if len(h)!=44 or h[:4]!=b'3DSX' or struct.unpack_from('<H',h,4)[0]<44:raise ValueError('Invalid extended 3DSX header.')
 smdh,size,rom=struct.unpack_from('<3I',h,32);end=path.stat().st_size
 if smdh<44 or not size or smdh+size>rom or rom>=end:raise ValueError('Invalid 3DSX SMDH/RomFS bounds.')
 with path.open('rb') as f:f.seek(smdh);magic=f.read(4)
 if magic!=b'SMDH':raise ValueError('Invalid embedded SMDH.')
 return {'bytes':end,'romfs_offset':rom,'smdh_bytes':size}

def build(root,jobs=0,clean=False):
 assets=verify(root);sdk=sdk_tools(root);source=root/'source';work=root/'build/obj';work.mkdir(parents=True,exist_ok=True)
 env=dict(os.environ,DEVKITPRO=sdk['base'].as_posix(),DEVKITARM=sdk['arm'].as_posix())
 for name in ['GCC_EXEC_PREFIX','COMPILER_PATH','LIBRARY_PATH','CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH']:env.pop(name,None)
 env['PATH']=os.pathsep.join(str(p) for p in [sdk['arm']/'bin',sdk['base']/'tools/bin',root/'tools/sdk/usr/bin'])+os.pathsep+env.get('PATH','')
 version=run([sdk['gcc'],'--version'],env,root).splitlines()[0]
 flags=[*ARCH,'-mword-relocations','-std=gnu11','-O2','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-D__3DS__','-DARM11','-DLUME_MEDIA_TOP=13','-DLUME_MEDIA_BOTTOM=226','-I'+str(source),'-I'+str(sdk['ctru']/'include'),'-I'+str(sdk['port']/'include')]
 # SDK headers/libraries and executable tools participate in cache identity.
 sdk_files=[sdk['gcc'],sdk['pack'],sdk['smdh']]
 for folder in [sdk['ctru']/'include',sdk['ctru']/'lib',sdk['port']/'include',sdk['port']/'lib']:
  sdk_files.extend(p for p in folder.rglob('*') if p.is_file() and p.suffix in {'.h','.a','.specs'})
 sdk_files.extend(sdk['arm'].rglob('3dsx.specs'))
 sdk_identity={p.relative_to(sdk['base']).as_posix():sha(p) for p in sorted(set(sdk_files))}
 units=source_units(source);signature=compile_signature(version+json.dumps(sdk_identity,sort_keys=True),flags,source)
 state_path=root/'build/objects.json'
 try:state=json.loads(state_path.read_text(encoding="utf-8")) if state_path.is_file() else {}
 except (ValueError,OSError):state={}
 new={};objects=[];tasks=[]
 for p in units:
  name=p.relative_to(source).as_posix()
  key=hashlib.sha256((signature+sha(p)).encode()).hexdigest();obj=object_path(work,name);objects.append(obj);new[name]=key
  cached=state.get(name,{})
  if clean or not isinstance(cached,dict) or cached.get('key')!=key or not obj.is_file() or cached.get('sha256')!=sha(obj):tasks.append((p,obj))
 def compile_one(task):
  p,obj=task;print('Compiling:',p.relative_to(source).as_posix(),flush=True);obj.parent.mkdir(parents=True,exist_ok=True);temp=obj.with_suffix('.o.part')
  try:run([sdk['gcc'],*flags,'-c',p,'-o',temp],env,root);os.replace(temp,obj)
  finally:temp.unlink(missing_ok=True)
 if not objects:raise ValueError('No C source files found.')
 with ThreadPoolExecutor(max_workers=jobs or min(4,os.cpu_count() or 1)) as pool:list(pool.map(compile_one,tasks))
 dump(state_path,{p.relative_to(source).as_posix():{'key':new[p.relative_to(source).as_posix()],'sha256':sha(object_path(work,p.relative_to(source).as_posix()))} for p in units})
 icon=root/'resources/icon.png'
 if not icon.is_file():icon=sdk['ctru']/'default_icon.png'
 recipe={'cache_policy':sha(root/'build/package_cache.py'),'builder':sha(Path(__file__)),'romfs_packer':sha(root/'build/romfs_compact.py')}
 package_key=package_signature({'recipe':recipe,'sdk':sdk_identity,'flags':flags,'objects':{p.relative_to(work).as_posix():sha(p) for p in objects},'assets':assets,'sources':source_inventory(root),'icon':sha(icon)})
 cached=None if clean else reusable(root/'out',package_key)
 if cached:
  if verify(root)!=assets:raise ValueError('Assets changed during compilation.')
  print('Verified; linking and packaging skipped:',cached,flush=True);return cached
 # Every output is staged. A running emulator cannot lock compiler cleanup.
 # If the stable output is locked, publish a dated alternative, retaining both.
 stage=root/'build/publish';stage.mkdir(exist_ok=True);elf=stage/'lume.elf';smdh=stage/'lume.smdh';game=stage/'lume.3dsx'
 link=[sdk['gcc'],*ARCH,'-specs=3dsx.specs','-g','-Wl,--gc-sections','-Wl,-Map,'+str(stage/'lume.map'),*objects,'-L'+str(sdk['port']/'lib'),'-L'+str(sdk['ctru']/'lib'),'-lcitro2d','-lcitro3d','-lmpg123','-ljpeg','-llz4','-lz','-lctru','-lm','-o',elf]
 run(link,env,root)
 icon=root/'resources/icon.png'
 if not icon.is_file():icon=sdk['ctru']/'default_icon.png'
 if not icon.is_file():raise ValueError('SDK default SMDH icon not found: '+str(icon))
 run([sdk['smdh'],'--create','Lume','Nintendo 3DS adaptation','Lume homebrew',icon,smdh],env,root)
 raw=stage/'lume.raw.3dsx'
 run([sdk['pack'],elf,raw,'--smdh='+str(smdh),'--romfs='+str(root/'romfs')],env,root)
 sharing=compact_3dsx(raw,game,assets)
 print('RomFS shared data:',sharing['saved_bytes'],'bytes saved;',sharing['files'],'files verified.')
 info=validate_3dsx(game);info['romfs_dedup']=sharing
 if verify(root)!=assets:raise ValueError('Assets changed during compilation; output was not published.')
 out=root/'out';out.mkdir(exist_ok=True);dest=out/'lume.3dsx';temp=out/'lume.3dsx.new';shutil_copy(game,temp)
 try:os.replace(temp,dest)
 except PermissionError:
  dest=out/('lume_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f')+'.3dsx');os.replace(temp,dest)
  print('Previous output is locked; saved a new build under a different name.')
 info.update(build_input_signature=package_key,file=dest.name,sha256=sha(dest),compiler=version,source=source_inventory(root),assets=assets)
 dump(out/'build-report.json',info);dump(root/'source-manifest.json',{'files':source_inventory(root)})
 print('READY:',dest,flush=True);return dest

def shutil_copy(source,dest):
 import shutil
 shutil.copyfile(source,dest)
 if sha(source)!=sha(dest):raise ValueError('Published file checksum verification failed.')
