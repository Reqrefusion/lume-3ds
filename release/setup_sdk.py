"""Link an existing devkitPro install; never installs or copies the SDK."""
from pathlib import Path
import os,subprocess,argparse
ROOT=Path(__file__).resolve().parents[1]
def setup(sdk,root=ROOT):
 sdk=Path(sdk).resolve();link=Path(root)/'tools/sdk/opt/devkitpro'
 for name in ['devkitARM/bin/arm-none-eabi-gcc'+('.exe' if os.name=='nt' else ''),'libctru/lib/libctru.a','portlibs/3ds/lib/libmpg123.a']:
  if not (sdk/name).is_file():raise ValueError('Incomplete existing SDK: '+name)
 if link.exists():
  if link.resolve()!=sdk:raise ValueError('Project already uses a different SDK')
  return
 link.parent.mkdir(parents=True,exist_ok=True)
 if os.name=='nt':subprocess.run(['cmd.exe','/d','/c','mklink','/J',str(link),str(sdk)],check=True)
 else:link.symlink_to(sdk,target_is_directory=True)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--sdk',required=True);a=p.parse_args();setup(a.sdk)
