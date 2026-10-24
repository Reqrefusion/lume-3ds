"""Compile the actual animated correspondence renderer and check both eyes."""
from pathlib import Path
import os,subprocess,tempfile,unittest
from test_fence_flow import ROOT,compiler,block

class MeterStereoTests(unittest.TestCase):
 def test_dynamic_frame_and_slider(self):
  cc=compiler()
  if not cc:self.skipTest('Native C compiler unavailable')
  source=ROOT/'source'
  text=(ROOT/'source/engine/assets/stereo_field.h').read_text()
  function=block(text,'static inline void lume_field_dynamic(')
  loader=block(text,'static inline void lume_field_close(')+'\n'+block(text,'static inline bool lume_field_load(')
  program=r'''
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
static int LZ4_decompress_safe(const char *a,char *b,int c,int d){(void)a;(void)b;(void)c;(void)d;return -1;}
typedef struct {int8_t *map;uint8_t *image;unsigned w,h,cached;bool valid;} LumeStereoField;
'''+loader+'\n'+function+r'''
#define CHECK(x) do {if(!(x))return __LINE__;}while(0)
int main(void){
 int8_t map[4]={4,4,4,4};
 uint8_t left[12]={0,0,0,40,40,40,80,80,80,120,120,120},out[12];
 LumeStereoField field={map,0,4,1,0,true};
 lume_field_dynamic(&field,left,out,0);CHECK(!memcmp(left,out,12));
 lume_field_dynamic(&field,left,out,128);CHECK(out[0]==20&&out[3]==60&&out[9]==120);
 lume_field_dynamic(&field,left,out,256);CHECK(out[0]==40&&out[3]==80&&out[9]==120);
 left[3]=100; /* Same slider, next animated frame: must never reuse cached pixels. */
 lume_field_dynamic(&field,left,out,256);CHECK(out[0]==100);
 map[0]=-4;lume_field_dynamic(&field,left,out,256);CHECK(out[0]==0);
 field.valid=false;lume_field_dynamic(&field,left,out,256);CHECK(!memcmp(left,out,12));
 /* Exercise a real failed loader, not a hand-built invalid field whose
    dimensions incorrectly remain initialized in the test. */
 field.map=NULL;field.image=NULL;
 CHECK(!lume_field_load(&field,"missing_stereo_asset",4,1));
 CHECK(!field.valid&&field.w==4&&field.h==1);
 memset(out,0xee,sizeof out);
 lume_field_dynamic(&field,left,out,256);CHECK(!memcmp(left,out,12));
 lume_field_dynamic(&field,left,out,0);CHECK(!memcmp(left,out,12));
 FILE *bad=fopen("broken.sdf","wb");CHECK(bad);
 CHECK(fwrite("SDF1bad packet",1,14,bad)==14);CHECK(!fclose(bad));
 CHECK(!lume_field_load(&field,"broken",4,1));
 memset(out,0xdd,sizeof out);lume_field_dynamic(&field,left,out,128);
 CHECK(!memcmp(left,out,12));
 lume_field_close(&field);
 puts("PASS: mono, half/full depth, frame change, boundary and missing field");return 0;
}
'''
  main=(source/'main.c').read_text()
  self.assertIn('lume_field_dynamic(&clue_field,meter_frame,rendered[i],lume_stereo_strength())',main)
  work=ROOT/'work';work.mkdir(exist_ok=True)
  with tempfile.TemporaryDirectory(prefix='meter_test_',dir=work) as folder:
   temp=Path(folder);test=temp/'test.c';test.write_text(program)
   exe=temp/('test.exe' if os.name=='nt' else 'test')
   path,env,msvc=cc
   args=([path,'/nologo','/std:c11','/MT',str(test),'/Fe:'+str(exe)] if msvc else [path,'-std=c11',str(test),'-o',str(exe)])
   build=subprocess.run(args,cwd=temp,env=env,capture_output=True,text=True)
   self.assertEqual(build.returncode,0,build.stdout+build.stderr)
   result=subprocess.run([str(exe)],cwd=temp,env=env,capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
if __name__=='__main__':unittest.main()
