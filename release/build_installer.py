"""Build the standalone Old/New 3DS setup homebrew using an existing SDK."""
from pathlib import Path
import argparse,os,subprocess
ROOT=Path(__file__).resolve().parents[1]
def build(sdk):
 sdk=Path(sdk).resolve();arm=sdk/'devkitARM/bin';work=ROOT/'build/installer';work.mkdir(parents=True,exist_ok=True);src=ROOT/'release/installer';ext='.exe' if os.name=='nt' else ''
 env=dict(os.environ,DEVKITPRO=sdk.as_posix(),DEVKITARM=(sdk/'devkitARM').as_posix());env['PATH']=str(arm)+os.pathsep+env.get('PATH','')
 for name in ['GCC_EXEC_PREFIX','COMPILER_PATH','LIBRARY_PATH','CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH']:env.pop(name,None)
 arch=['-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft']
 flags=[*arch,'-O2','-g','-ffunction-sections','-fdata-sections','-DAES256=1','-DECB=0','-DCTR=0','-D__3DS__','-DARM11','-I'+str(sdk/'libctru/include'),'-I'+str(sdk/'portlibs/3ds/include')]
 commands=[
 [arm/('arm-none-eabi-gcc'+ext),*flags,'-c',ROOT/'third_party/installer/tiny-AES-c/aes.c','-o',work/'aes.o'],
 [arm/('arm-none-eabi-g++'+ext),*flags,'-std=gnu++11','-Wall','-Wextra','-Werror','-c',src/'main.cpp','-o',work/'main.o'],
 [arm/('arm-none-eabi-g++'+ext),*arch,'-specs=3dsx.specs','-Wl,--gc-sections',work/'main.o',work/'aes.o','-L'+str(sdk/'portlibs/3ds/lib'),'-L'+str(sdk/'libctru/lib'),'-lminizip','-lz','-lctru','-lm','-o',work/'lume-setup.elf'],
 [sdk/('tools/bin/smdhtool'+ext),'--create','Lume Setup','Install ready Lume 3DS assets','Lume homebrew',sdk/'libctru/default_icon.png',work/'lume-setup.smdh'],
 [sdk/('tools/bin/3dsxtool'+ext),work/'lume-setup.elf',work/'lume-setup.3dsx','--smdh='+str(work/'lume-setup.smdh')]]
 for command in commands:subprocess.run([str(v).replace('\\','/') for v in command],env=env,check=True)
 print('READY:',work/'lume-setup.3dsx');return work/'lume-setup.3dsx'
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--sdk',default=str(ROOT/'tools/sdk/opt/devkitpro'));a=p.parse_args();build(a.sdk)
