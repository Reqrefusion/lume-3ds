"""Import already-produced RomFS and private game data; no model/video conversion."""
from pathlib import Path,PurePosixPath
import argparse,sys,shutil,os,json,struct,tempfile
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'build'))
from romfs_compact import read_index,inventory
from manifest import save

def run(game,private,root):
 game,private,root=Path(game).resolve(),Path(private).resolve(),Path(root).resolve()
 target=root/'romfs'
 if target.exists():raise ValueError('RomFS already exists; use a new source checkout.')
 for name in ['original_font.h','icon.png']:
  if not (private/name).is_file():raise ValueError('Missing developer data: '+name)
 expected=inventory(game);base,start,_,files=read_index(game)
 target.parent.mkdir(parents=True,exist_ok=True)
 with tempfile.TemporaryDirectory(prefix='ready-assets-',dir=target.parent) as temp:
  stage=Path(temp)/'romfs';stage.mkdir()
  with game.open('rb') as stream:
   for name,offset,size,_ in files:
    rel=PurePosixPath(name)
    if rel.is_absolute() or '..' in rel.parts or ':' in name or '\\' in name:raise ValueError('Unsafe RomFS name')
    dest=stage.joinpath(*rel.parts);dest.parent.mkdir(parents=True,exist_ok=True);stream.seek(base+start+offset)
    with dest.open('xb') as out:
     while size:
      block=stream.read(min(size,1024*1024))
      if not block:raise ValueError('Truncated RomFS')
      out.write(block);size-=len(block)
  import hashlib
  actual={p.relative_to(stage).as_posix():{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in stage.rglob('*') if p.is_file()}
  if actual!=expected:raise ValueError('Asset extraction mismatch')
  shutil.copyfile(private/'original_font.h',root/'source/game/lume/original_font.h')
  (root/'resources/icon.png').parent.mkdir(parents=True,exist_ok=True)
  shutil.copyfile(private/'icon.png',root/'resources/icon.png')
  if (private/'languages').is_dir():shutil.copytree(private/'languages',root/'languages',dirs_exist_ok=True)
  if (private/'actionscript').is_dir():shutil.copytree(private/'actionscript',root/'docs/actionscript',dirs_exist_ok=True)
  if (private/'Lume.swf').is_file():
   (root/'tools/detail').mkdir(parents=True,exist_ok=True);shutil.copyfile(private/'Lume.swf',root/'tools/detail/Lume.swf')
  os.replace(stage,target)
 save(root);print('Ready 3D assets imported:',len(expected),'(no conversion)')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--game',required=True);p.add_argument('--private',required=True);p.add_argument('--root',default=str(ROOT));a=p.parse_args();run(a.game,a.private,a.root)
