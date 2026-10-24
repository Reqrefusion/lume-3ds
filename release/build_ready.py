"""Developer build of the fixed SD-data app; no original media is embedded."""
from pathlib import Path
import sys,argparse
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'build'));sys.path.insert(0,str(ROOT/'scripts/exe'));sys.path.insert(0,str(ROOT/'release'))
from build_player_runtime import main as build_app
from state import Lock
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--sdk',default=str(ROOT/'tools/sdk/opt/devkitpro'));a=p.parse_args()
 with Lock(ROOT/'tools/exe-production/production.lock'):
  build_app(a.sdk)
  if (ROOT/'romfs').is_dir():
   from manifest import verify
   from player_content import publish
   publish(ROOT/'player/lume.3dsx',ROOT/'romfs',ROOT/'out',verify(ROOT),{'developer_build':True,'game_binary_generated':False})
