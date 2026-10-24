"""Package the development repository and optional geometry-only release assets."""
from pathlib import Path
import argparse,hashlib,json,shutil,sys,zipfile
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'release'))
from export_maps import package_prepared,prepared_files,sha

TEXT_EXTENSIONS={'.py','.java','.json','.h','.hpp','.cpp','.c','.inc','.ps1','.cmd','.md','.txt','.yml','.yaml','.cmake'}
PRIVATE_PARTS={'__pycache__','obj','publish','metadata','dist','private'}
PRIVATE_FILES={'original_font.h'}

def zip_tree(folder,dest):
 with zipfile.ZipFile(dest,'x',zipfile.ZIP_DEFLATED,compresslevel=1) as archive:
  for path in sorted(folder.rglob('*')):
   if path.is_file():archive.write(path,path.relative_to(folder).as_posix())

def export_source(source):
 source=Path(source);source.mkdir()
 for folder in ['source','scripts','build','release','third_party','docs','.github']:
  for path in sorted((ROOT/folder).rglob('*')):
   relative=path.relative_to(ROOT)
   if not path.is_file() or path.name in PRIVATE_FILES or PRIVATE_PARTS.intersection(relative.parts):continue
   if path.is_symlink():raise ValueError('Source export does not follow symbolic links: '+str(relative))
   if folder=='build' and (path.parent!=ROOT/'build' or path.suffix=='.json'):continue
   if relative.parts[:2]==('docs','actionscript'):continue
   upstream=folder=='third_party' and (path.name.endswith('.tar.bz2') or path.name in {'README','LICENSE','COPYING','AUTHORS'})
   if path.suffix.lower() not in TEXT_EXTENSIONS and not upstream:continue
   target=source/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,target)
 for name in ['README.md','LICENSE','COPYRIGHT.md','.gitignore','video-settings.json','BUILD.cmd','PRODUCE.cmd','SETUP.cmd','UPDATE_LANGUAGES.cmd']:
  shutil.copyfile(ROOT/name,source/name)
 (source/'resources').mkdir();shutil.copyfile(ROOT/'resources/icon.png',source/'resources/icon.png')
 (source/'languages').mkdir()
 for path in sorted((ROOT/'languages').glob('*.json')):
  language=json.loads(path.read_text(encoding='utf-8-sig'))
  (source/'languages'/path.name).write_text(json.dumps(language,ensure_ascii=False,indent=2),encoding='utf-8')
 for path in (ROOT/'languages').glob('*.txt'):shutil.copyfile(path,source/'languages'/path.name)
 runtime=json.loads((ROOT/'player/runtime.json').read_text(encoding='utf-8'))
 if runtime.get('storage')!='external_sd' or runtime.get('original_game_content_embedded') is not False or sha(ROOT/'player/lume.3dsx')!=runtime['sha256']:raise ValueError('Application must load external game content and match its manifest')
 (source/'player').mkdir()
 for name in ['lume.3dsx','runtime.json']:shutil.copyfile(ROOT/'player'/name,source/'player'/name)
 for name in prepared_files(ROOT):
  target=source/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/name,target)
 manifest={path.relative_to(source).as_posix():sha(path) for path in source.rglob('*') if path.is_file()}
 (source/'PUBLIC_SOURCE_MANIFEST.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
 return manifest

def run(dest,source_only=False):
 dest=Path(dest).resolve()
 if dest==ROOT or dest.is_relative_to(ROOT):raise ValueError('Choose an output directory outside the source repository')
 if dest.exists():raise ValueError('Use a new output directory')
 if source_only:dest.mkdir(parents=True)
 else:package_prepared(ROOT,dest)
 source=dest/'Lume3DS-Source';export_source(source);zip_tree(source,dest/'Lume3DS-Source.zip')
 if not source_only:
  shutil.copyfile(dest/'Lume3DS-Source.zip',dest/'Lume3DS-Player-Setup.zip')
  app=dest/'App/3ds';app.mkdir(parents=True);shutil.copyfile(ROOT/'player/lume.3dsx',app/'lume.3dsx');zip_tree(dest/'App',dest/'Lume3DS-App.zip')
 (dest/'SHA256SUMS.txt').write_text(''.join(sha(path)+'  '+path.name+'\n' for path in sorted(dest.glob('*.zip'))),encoding='ascii')
 print('Source and development tools packaged:',source)

if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dest',required=True);parser.add_argument('--source-only',action='store_true');args=parser.parse_args();run(args.dest,args.source_only)
