"""Cross-check the console decoder against the .NET writer using owned local input."""
from pathlib import Path
import argparse,hashlib,os,subprocess,sys,tempfile
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts/tests'))
from test_fence_flow import compiler

def run(original):
 original=Path(original).resolve();found=compiler()
 if not found:raise RuntimeError('Host C/C++ compiler required')
 cc,env,msvc=found
 with tempfile.TemporaryDirectory(prefix='lume-native-test-') as temp:
  temp=Path(temp);exe=temp/('unseal.exe' if msvc else 'unseal')
  sources=[ROOT/'release/installer/host.cpp',ROOT/'third_party/installer/tiny-AES-c/aes.c']
  if msvc:command=[cc,'/nologo','/O2','/EHsc','/std:c++14','/DAES256=1','/DECB=0','/DCTR=0',*map(str,sources),'/Fe:'+str(exe)]
  else:
   import shutil
   cxx=shutil.which('c++') or shutil.which('g++') or shutil.which('clang++')
   command=[cxx,'-O2','-std=c++11','-DAES256=1','-DECB=0','-DCTR=0',*map(str,sources),'-o',str(exe)]
  subprocess.run(command,cwd=temp,env=env,check=True)
  payload=temp/'payload.bin';payload.write_bytes(bytes(range(256))*800+b'final non-aligned bytes')
  capsule=temp/'fixture.l3dp'
  subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(ROOT/'release/Create-Package.ps1'),'-OriginalExe',str(original),'-Payload',str(payload),'-Package',str(capsule)],check=True)
  output=temp/'output.bin';subprocess.run([str(exe),str(original),str(capsule),str(output)],check=True)
  assert output.read_bytes()==payload.read_bytes();checks=1
  def fails(src,pkg,dest,extra=()):
   result=subprocess.run([str(exe),str(src),str(pkg),str(dest),*extra],capture_output=True,text=True)
   assert result.returncode!=0 and not dest.exists(),result.stderr
  wrong=temp/'wrong.exe';data=bytearray(original.read_bytes());data[100]^=1;wrong.write_bytes(data)
  fails(wrong,capsule,temp/'wrong-output');checks+=1
  for offset in [50,120]:
   data=bytearray(capsule.read_bytes());data[offset]^=1;bad=temp/('bad-'+str(offset));bad.write_bytes(data)
   fails(original,bad,temp/('output-'+str(offset)));checks+=1
  short=temp/'truncated';short.write_bytes(capsule.read_bytes()[:-1]);fails(original,short,temp/'short-output');checks+=1
  result=subprocess.run([str(exe),str(original),str(capsule),str(output)],capture_output=True)
  assert result.returncode!=0 and output.read_bytes()==payload.read_bytes();checks+=1
  for phase in ['Checking original','Verifying package','Opening ready assets']:
   fails(original,capsule,temp/('cancel-'+str(checks)),[phase]);checks+=1
  print('PASS native package checks:',checks,'(roundtrip, original, authentication, truncation, overwrite, cancellation cleanup)')
  return checks
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--original',required=True);a=p.parse_args();run(a.original)
