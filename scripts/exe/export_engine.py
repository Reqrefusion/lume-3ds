"""Export the reusable engine subtree without game code or original media."""
from pathlib import Path
import argparse,hashlib,json,shutil
ROOT=Path(__file__).resolve().parents[2]
def export(source,dest):
 source,dest=Path(source).resolve(),Path(dest).resolve()
 common=source/'engine' if (source/'engine/core/jobs_state.c').is_file() else source
 if not (common/'core/jobs_state.c').is_file():raise ValueError('Canonical engine source not found')
 if dest==source or dest.is_relative_to(source) or dest==common or dest.is_relative_to(common) or dest.exists():raise ValueError('Choose a new export directory outside the source tree')
 dest.mkdir(parents=True);shutil.copytree(common,dest/'source/engine')
 template=Path(__file__).with_name('engine_template')
 for name in ['build_3ds.py','examples','USAGE.md']:
  item=template/name
  if item.is_dir():shutil.copytree(item,dest/name)
  else:shutil.copyfile(item,dest/name)
 shutil.copyfile(ROOT/'build/romfs_compact.py',dest/'romfs_compact.py')
 shutil.copyfile(ROOT/'LICENSE',dest/'LICENSE')
 (dest/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.16)\nproject(shared_adventure_runtime C)\nadd_subdirectory(source/engine)\n')
 (dest/'runtime-profile.json').write_text(json.dumps({'surface':[320,240],'media_top':0,'media_bottom':240,'game_assets_included':False,'game_rules_included':False},indent=2))
 shutil.copyfile(common/'README.md',dest/'README.md')
 manifest={p.relative_to(dest).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in dest.rglob('*') if p.is_file()}
 (dest/'export-manifest.json').write_text(json.dumps(manifest,indent=2));return dest
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--dest',type=Path,required=True);a=p.parse_args();print(export(ROOT/'source',a.dest))
