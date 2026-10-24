"""Prepare the single application file for a GitHub release."""
from pathlib import Path
import argparse,json,shutil,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'release'))
from export_maps import prepared_files,sha

TEXT_EXTENSIONS={'.py','.java','.json','.h','.hpp','.cpp','.c','.inc','.ps1','.cmd','.md','.txt','.yml','.yaml','.cmake'}
PRIVATE_PARTS={'__pycache__','obj','publish','metadata','dist','private'}
PRIVATE_FILES={'original_font.h'}

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

def run(dest):
 dest=Path(dest).resolve()
 if dest==ROOT or dest.is_relative_to(ROOT):raise ValueError('Choose an output directory outside the source repository')
 if dest.exists():raise ValueError('Use a new output directory')
 app=ROOT/'player/lume.3dsx';runtime=json.loads((ROOT/'player/runtime.json').read_text(encoding='utf-8'))
 if runtime.get('storage')!='external_sd' or runtime.get('original_game_content_embedded') is not False or sha(app)!=runtime['sha256']:raise ValueError('Application integrity check failed')
 for name,record in runtime['source'].items():
  if sha(ROOT/name)!=record['sha256']:raise ValueError('Native source changed; rebuild the application first')
 dest.mkdir(parents=True);target=dest/'lume.3dsx';shutil.copyfile(app,target)
 if sha(target)!=runtime['sha256']:raise ValueError('Release copy differs from the application')
 print('Release file ready:',target)
 return target

if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dest',required=True);args=parser.parse_args();run(args.dest)
