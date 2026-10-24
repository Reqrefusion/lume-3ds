"""Build source, SD-card and optional Windows downloads with existing ready assets."""
from pathlib import Path
import argparse,json,shutil,subprocess,sys,tempfile,zipfile
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'build'))
from manifest import verify,sha
sys.path.insert(0,str(ROOT/'release'))
from public_layout import public_text,public_path

def copy(src,dst):
 dst.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(src,dst)
def zip_tree(folder,dest):
 with zipfile.ZipFile(dest,'w',zipfile.ZIP_DEFLATED,compresslevel=1) as z:
  for p in sorted(folder.rglob('*')):
   if p.is_file():z.write(p,p.relative_to(folder).as_posix())
def export_source(source):
 source.mkdir();allowed={'.py','.java','.json','.h','.hpp','.cpp','.c','.inc','.ps1','.cs','.cmd','.md','.txt','.yml','.cmake'}
 obsolete={'public_layout.py','original_font.h'}
 for folder in ['source','scripts','build','release','third_party']:
  for p in (ROOT/folder).rglob('*'):
   rel=p.relative_to(ROOT)
   if not p.is_file() or p.name in obsolete or any(x in rel.parts for x in ['__pycache__','obj','publish','metadata','dist','private','commands','installer-host']):continue
   if folder=='build' and (p.parent!=ROOT/'build' or p.suffix=='.json'):continue
   if p.suffix.lower() not in allowed and not(folder=='third_party' and p.name.endswith('.tar.bz2')):continue
   target=source/str(public_path(rel.as_posix()));target.parent.mkdir(parents=True,exist_ok=True)
   if p.name.endswith('.bz2'):copy(p,target)
   else:target.write_text(public_text(p.read_text(encoding='utf-8-sig')),encoding='utf-8')
 for name in ['LICENSE','COPYRIGHT.md','.gitignore','video-settings.json']:
  if (ROOT/name).is_file():(source/str(public_path(name))).write_text(public_text((ROOT/name).read_text(encoding='utf-8-sig')),encoding='utf-8')
 for p in (ROOT/'languages').glob('*.json'):
  data=json.loads(p.read_text(encoding='utf-8-sig'));data['text']={};out=source/'languages'/p.name;out.parent.mkdir(exist_ok=True);out.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
 for name in ['BUILDING','INSTALLING','PUBLISHING']:
  copy(ROOT/f'release/{name}.md',source/f'docs/{name}.md')
 copy(ROOT/'release/SOURCE_README.md',source/'README.md')
 workflow=source/'.github/workflows/check.yml';workflow.parent.mkdir(parents=True,exist_ok=True);workflow.write_text("name: Source checks\non: [push, pull_request]\njobs:\n  checks:\n    runs-on: windows-latest\n    steps:\n      - uses: actions/checkout@v4\n      - uses: actions/setup-python@v5\n        with:\n          python-version: '3.12'\n      - name: Check Python syntax\n        run: python -c \"import ast,pathlib; [ast.parse(p.read_text(encoding='utf-8-sig')) for p in pathlib.Path('.').rglob('*.py')]\"\n      - name: Check package authentication\n        shell: powershell\n        run: ./release/tests/test_package.ps1 -WorkDirectory work/package-check\n",encoding='utf-8')
 copy(ROOT/'docs/actionscript-mapping.json',source/'docs/actionscript-mapping.json')
 mapping=source/'docs/actionscript-mapping.json';mapping.write_text(public_text(mapping.read_text()),encoding='utf-8')
 # The public tree is canonical already. Future releases need no legacy mapping.
 (source/'release/public_layout.py').write_text('from pathlib import PurePosixPath\ndef public_text(text):return text\ndef public_path(path):return PurePosixPath(path)\n',encoding='ascii')
 # Prevent old private-workspace assumptions in reusable engine export.
 engine=source/'scripts/exe/export_engine.py';s=engine.read_text().replace('source.parents[2]','source.parent');s=s.replace("notices=source.parent/'tools/gpu-video-dependencies'","notices=source.parent/'third_party'");engine.write_text(s,encoding='utf-8')
 pipeline=source/'scripts/exe/pipeline.py';s=pipeline.read_text();s=s.replace("    shutil.move(str(dest/'romfs'),str(dest/'romfs'))",'    # RomFS is already at the canonical staging location.');s=s.replace("['tools','resources','build','backups','languages']","['tools','source','resources','build','backups','languages']");pipeline.write_text(s,encoding='utf-8')
 # Drop names of obsolete local migration tools from the public builder itself.
 # Use the public bootstrap, with an existing SDK rather than historical kit migration.
 copy(ROOT/'release/produce.ps1',source/'build/produce.ps1')
 for name,command in {'BUILD':'release\\build_ready.py','BUILD_INSTALLER':'release\\build_installer.py'}.items():
  (source/(name+'.cmd')).write_bytes(('@echo off\r\npy -3.12 "%~dp0'+command+'" %*\r\n').encode('ascii'))
 (source/'PRODUCE.cmd').write_bytes(b'@echo off\r\npowershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build\\produce.ps1" %*\r\n')
 forbidden={'.exe','.swf','.3dsx','.l3dp','.png','.pcm','.mp3','.lan','.lvs','.dzp','.rgba','.rgb','.sdf','.onnx','.pth','.class','.a','.o'}
 for p in source.rglob('*'):
  if p.is_file() and (p.suffix.lower() in forbidden or 'romfs' in p.relative_to(source).parts):raise ValueError('Private asset in public source: '+str(p))
 manifest={p.relative_to(source).as_posix():sha(p) for p in source.rglob('*') if p.is_file()}
 (source/'PUBLIC_SOURCE_MANIFEST.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8');return manifest

def run(dest,original):
 dest=Path(dest).resolve();original=Path(original).resolve()
 if dest.exists():raise ValueError('Use a new output folder')
 if sha(original)!='ba7a947b9a501a1723508e388764113e27b85db7bc14f52b613473373cad0089':raise ValueError('Unsupported original EXE')
 assets=verify(ROOT);game=ROOT/'out/lume.3dsx';report=json.loads((ROOT/'out/build-report.json').read_text(encoding='utf-8-sig'))
 if sha(game)!=report['sha256']:raise ValueError('Build checksum mismatch')
 installer=ROOT/'build/installer/lume-setup.3dsx';smdh=installer.with_suffix('.smdh')
 if not installer.is_file() or not smdh.is_file():raise ValueError('Build the console installer first')
 dest.mkdir(parents=True);source=dest/'Lume3DS-Source';manifest=export_source(source);zip_tree(source,dest/'Lume3DS-Source.zip')
 with tempfile.TemporaryDirectory(prefix='lume-release-') as tmp:
  tmp=Path(tmp);windows=tmp/'windows';windows.mkdir();payload=tmp/'ready.zip'
  with zipfile.ZipFile(payload,'w',zipfile.ZIP_DEFLATED,compresslevel=1) as z:
   z.write(game,'lume.3dsx')
   for path,name in [(ROOT/'source/game/lume/original_font.h','original_font.h'),(ROOT/'resources/icon.png','icon.png'),(ROOT/'build/publish/lume.smdh','lume.smdh'),(ROOT/'out/build-report.json','build-report.json')]:z.write(path,'developer/'+name)
   for p in (ROOT/'languages').glob('*.json'):z.write(p,'developer/languages/'+p.name)
   for p in (ROOT/'build/obj').rglob('*.o'):z.write(p,'developer/obj/'+p.relative_to(ROOT/'build/obj').as_posix())
   for p in (ROOT/'docs/actionscript').rglob('*.as'):z.write(p,'developer/actionscript/'+p.relative_to(ROOT/'docs/actionscript').as_posix())
   z.write(ROOT/'tools/detail/Lume.swf','developer/Lume.swf')
  for old,new in [('Install.cmd','Install.cmd'),('Install.ps1','Install.ps1'),('package_core.cs','package_core.cs')]:
   target=windows/new;target.write_text(public_text((ROOT/'release'/old).read_text()),encoding='utf-8')
  copy(ROOT/'release/PLAYER_README.txt',windows/'README.txt')
  for name in ['LICENSE','COPYRIGHT.md']:copy(ROOT/name,windows/name)
  shutil.copytree(ROOT/'third_party',windows/'third_party')
  subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(ROOT/'release/Create-Package.ps1'),'-OriginalExe',str(original),'-Payload',str(payload),'-Package',str(windows/'Lume3DS.l3dp')],check=True)
  zip_tree(windows,dest/'Lume3DS-Windows.zip')
  sd=tmp/'sdcard';setup=sd/'3ds/lume-setup';setup.mkdir(parents=True)
  for p in [installer,smdh,windows/'Lume3DS.l3dp']:copy(p,setup/p.name)
  copy(ROOT/'release/INSTALLING.md',sd/'README.md')
  for name in ['LICENSE','COPYRIGHT.md']:copy(ROOT/name,sd/name)
  shutil.copytree(ROOT/'third_party',sd/'third_party')
  zip_tree(sd,dest/'Lume3DS-SDCard.zip')
 sums={p.name:sha(p) for p in dest.glob('*.zip')}
 (dest/'SHA256SUMS.txt').write_text(''.join(h+'  '+n+'\n' for n,h in sums.items()),encoding='ascii')
 (dest/'RELEASE_CHECKS.json').write_text(json.dumps({'source_files':len(manifest),'ready_assets':len(assets),'game_sha256':sha(game),'original_exe_included':False,'models_run':False,'uploaded':False,'physical_console_install_tested':False,'license':'GPL-3.0-only','downloads':sums},indent=2),encoding='utf-8')
 print('READY:',dest)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--dest',required=True);p.add_argument('--original',default=str(ROOT/'Lume.exe'));a=p.parse_args();run(a.dest,a.original)
