"""Relink application objects with a replacement mpg123 library; no asset conversion."""
from pathlib import Path
import argparse,os,subprocess,sys,tempfile
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'build'))
from romfs_compact import compact_3dsx
if __name__=='__main__':
 p=argparse.ArgumentParser()
 for n in ['sdk','private','romfs','mpg123-lib','out']:p.add_argument('--'+n,required=True)
 a=p.parse_args();sdk=Path(a.sdk).resolve();private=Path(a.private).resolve();out=Path(a.out).resolve();objects=list((private/'obj').rglob('*.o'))
 if out.exists() or not objects:raise ValueError('New output and application objects required')
 arm=sdk/'devkitARM/bin';env=os.environ.copy();env['PATH']=str(arm)+os.pathsep+env['PATH'];env['DEVKITPRO']=str(sdk)
 with tempfile.TemporaryDirectory() as t:
  elf=Path(t)/'lume.elf';raw=Path(t)/'lume.3dsx'
  subprocess.run([str(arm/'arm-none-eabi-g++.exe'),'-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft','-specs=3dsx.specs','-Wl,--gc-sections',*map(str,objects),'-L'+str(sdk/'portlibs/3ds/lib'),'-L'+str(sdk/'libctru/lib'),'-lcitro2d','-lcitro3d',a.mpg123_lib,'-ljpeg','-llz4','-lz','-lctru','-lm','-o',str(elf)],env=env,check=True)
  subprocess.run([str(sdk/'tools/bin/3dsxtool.exe'),str(elf),str(raw),'--smdh='+str(private/'lume.smdh'),'--romfs='+str(Path(a.romfs).resolve())],check=True)
  compact_3dsx(raw,out)