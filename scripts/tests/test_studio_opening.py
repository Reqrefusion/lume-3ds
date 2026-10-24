"""The lower-screen studio follows opening time without delaying menu input."""
from pathlib import Path
import os,subprocess,tempfile,unittest,sys,struct,zlib
from test_fence_flow import ROOT,compiler
class StudioOpeningTests(unittest.TestCase):
 def test_clock_fade_and_independent_bottom_buffer(self):
  cc=compiler()
  if not cc:self.skipTest('Native C compiler unavailable')
  program=r"""
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "game/lume/title_animation.h"
#define CHECK(x) do{if(!(x)){printf("FAIL %d\n",__LINE__);return 1;}}while(0)
int main(void){
 CHECK(lume_title_studio_frame(0,224)==0&&lume_title_studio_frame(120,224)==120);
 CHECK(lume_title_studio_frame(223,224)==223&&lume_title_studio_frame(224,224)==223);
 CHECK(lume_title_studio_frame(UINT64_MAX,224)==223&&lume_title_studio_frame(0,0)==0);
 CHECK(lume_title_menu_opacity(195)==0&&lume_title_menu_opacity(234)==255);
 CHECK(!lume_title_touch_ready(236)&&lume_title_touch_ready(237));
 uint8_t top[6]={10,20,30,40,50,60},reference[6],studio[6]={200,100,50,200,100,50},menu[6]={40,20,10,40,20,10},bottom[6];
 memcpy(reference,top,6);memcpy(bottom,menu,6);lume_title_menu_blend(bottom,studio,6,0);CHECK(!memcmp(bottom,studio,6));
 memcpy(bottom,menu,6);lume_title_menu_blend(bottom,studio,6,255);CHECK(!memcmp(bottom,menu,6));
 memcpy(bottom,menu,6);lume_title_menu_blend(bottom,studio,6,lume_title_menu_opacity(215));
 for(unsigned i=0;i<6;i++)CHECK(bottom[i]>menu[i]&&bottom[i]<studio[i]);
 CHECK(!memcmp(top,reference,6));puts("PASS studio opening: one-shot clock, menu fade, unchanged touch timing and top image");return 0;
}
"""
  with tempfile.TemporaryDirectory(prefix='studio_opening_',dir=ROOT/'work') as folder:
   temp=Path(folder);test=temp/'test.c';test.write_text(program,encoding='utf-8');exe=temp/('test.exe' if os.name=='nt' else 'test');path,env,msvc=cc;source=ROOT/'source'
   args=([path,'/nologo','/std:c11','/utf-8','/O2','/MT','/I'+str(source),str(test),'/Fe:'+str(exe)] if msvc else [path,'-O2','-std=c11','-I'+str(source),str(test),'-o',str(exe)])
   result=subprocess.run(args,cwd=temp,env=env,capture_output=True,text=True);self.assertEqual(result.returncode,0,result.stdout+result.stderr)
   result=subprocess.run([str(exe)],cwd=temp,env=env,capture_output=True,text=True);self.assertEqual(result.returncode,0,result.stdout+result.stderr);print(result.stdout.strip())
 def test_production_validator_accepts_only_studio_low_resolution(self):
  sys.path.insert(0,str(ROOT/'scripts'))
  from toolchain import native_valid,read_lan
  raw=bytes([0,0,0,255])*(320*240);body=zlib.compress(raw)
  packet=struct.pack('<IhhHH',4<<16,0,0,320,240)+body
  data=struct.pack('<4s4I2I',b'LAN2',320,240,25,1,28,28+len(packet))+packet
  with tempfile.TemporaryDirectory(dir=ROOT/'work') as folder:
   studio=Path(folder)/'studio_intro.lan';studio.write_bytes(data)
   native_valid(studio);self.assertEqual(len(read_lan(studio,full=True,dimensions=(320,240))),1)
   other=Path(folder)/'other.lan';other.write_bytes(data)
   with self.assertRaises(ValueError):native_valid(other)
if __name__=='__main__':unittest.main()
