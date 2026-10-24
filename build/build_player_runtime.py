"""Developer-only: compile redistributable game code without original game assets."""
from pathlib import Path
import argparse,json,shutil,struct,sys,tempfile,hashlib
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'build'));sys.path.insert(0,str(ROOT/'release'))
from manifest import save,source_inventory,sha
from build_game import build

def main(sdk):
 with tempfile.TemporaryDirectory(prefix='lume-runtime-') as temp:
  stage=Path(temp);shutil.copytree(ROOT/'source',stage/'source',ignore=shutil.ignore_patterns('original_font.h'))
  icon=ROOT/'resources/icon.png'
  if not icon.is_file():raise ValueError('Missing application icon: '+str(icon))
  (stage/'resources').mkdir();shutil.copyfile(icon,stage/'resources/icon.png')
  (stage/'romfs').mkdir();(stage/'romfs/README.txt').write_text('Original game media must be generated locally. This placeholder is not player content.\n',encoding='ascii')
  (stage/'build').mkdir()
  for p in (ROOT/'build').glob('*.py'):shutil.copyfile(p,stage/'build'/p.name)
  # Connect an existing SDK only; this command is never run by players.
  target=stage/'tools/sdk/opt/devkitpro';target.parent.mkdir(parents=True)
  import os,subprocess
  if os.name=='nt':subprocess.run(['cmd','/d','/c','mklink','/J',str(target),str(Path(sdk).resolve())],check=True)
  else:target.symlink_to(Path(sdk).resolve(),target_is_directory=True)
  save(stage);game=build(stage);data=game.read_bytes();base=struct.unpack_from('<I',data,40)[0];app=bytearray(data[:base]);struct.pack_into('<I',app,40,0);app=bytes(app)
  dest=ROOT/'player';dest.mkdir(exist_ok=True);(dest/'lume.3dsx').write_bytes(app)
  (dest/'runtime.json').write_text(json.dumps({'format':2,'storage':'external_sd','sha256':hashlib.sha256(app).hexdigest(),'bytes':len(app),'original_media_included':True,'original_game_content_embedded':False,'original_logo_included':True,'original_font_included':False,'icon':'resources/icon.png','icon_sha256':sha(icon),'source':source_inventory(ROOT)},indent=2),encoding='utf-8')
  output=ROOT/'out/3ds';output.mkdir(parents=True,exist_ok=True);shutil.copyfile(dest/'lume.3dsx',output/'lume.3dsx')
  print('Fixed external-data app built:',len(app),'bytes')
if __name__=='__main__':
 import os
 default_sdk=os.environ.get('DEVKITPRO') or ('C:/devkitPro' if os.name=='nt' else '/opt/devkitpro')
 p=argparse.ArgumentParser();p.add_argument('--sdk',default=default_sdk);a=p.parse_args();main(a.sdk)