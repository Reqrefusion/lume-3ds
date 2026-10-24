"""Compare incremental staging with the unchanged full-frame scaler byte for byte."""
from pathlib import Path
import os,subprocess,tempfile,unittest
from test_fence_flow import ROOT,compiler

class PresentCacheTests(unittest.TestCase):
 def test_exact_frames_both_eyes_and_physical_buffers(self):
  cc=compiler()
  if not cc:self.skipTest('Native C compiler unavailable')
  source=ROOT/'source'
  program=r'''
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#define LUME_MEDIA_TOP 13
#define LUME_MEDIA_BOTTOM 226
#include "engine/render/blit.h"
#include "platform/3ds/present.inc"
#define CHECK(x) do {if(!(x)){printf("FAIL line %d\n",__LINE__);return 1;}}while(0)
static uint8_t source_rgb[2][230400],expected[288000],physical[2][2][288000];
static uint32_t random_state=17;
static uint8_t rnd(void){random_state=random_state*1664525u+1013904223u;return (uint8_t)(random_state>>24);}
int main(void){
 stage_prepare();
 for(unsigned eye=0;eye<2;eye++)for(unsigned p=0;p<230400;p++)source_rgb[eye][p]=rnd();
 for(unsigned frame=0;frame<48;frame++)for(unsigned eye=0;eye<2;eye++){
  unsigned extended=(frame/8)%2;
  if(frame%8==0)for(unsigned p=0;p<230400;p++)source_rgb[eye][p]=rnd();
  else if(frame%3)for(unsigned y=80;y<104;y++)for(unsigned x=130;x<154;x++)
    for(unsigned c=0;c<3;c++)source_rgb[eye][(y*320+x)*3+c]=rnd();
  if(frame%7==0)source_rgb[eye][(13*320+319)*3]^=255;
  uint8_t *fb=physical[eye][frame%2];memset(fb,0x81,288000);
  top_stage_cached(source_rgb[eye],fb,extended,eye);
  stage_borders(expected);top_stage_range(source_rgb[eye],expected,extended,0,240);
  CHECK(!memcmp(fb,expected,288000));
 }
 unsigned full,partial,reused,saved;
 lume_present_filter_stats(&full,&partial,&reused,&saved);
 CHECK(full>0&&partial>0&&reused>0&&saved>10000);
 /* Cache must contain the scene, never a card added after staging. */
 memset(physical[0][0],255,288000);
 top_stage_cached(source_rgb[0],physical[0][0],1,0);
 stage_borders(expected);top_stage_range(source_rgb[0],expected,1,0,240);
 CHECK(!memcmp(physical[0][0],expected,288000));
 /* Same stream, small actor updates: report host CPU timing as diagnostic,
    never equate it with measured ARM/Azahar performance. */
 clock_t begin=clock();
 for(unsigned i=0;i<100;i++){source_rgb[0][(90*320+140)*3]^=1;stage_borders(expected);top_stage_range(source_rgb[0],expected,1,0,240);}
 double full_ms=1000.0*(clock()-begin)/CLOCKS_PER_SEC;
 begin=clock();
 for(unsigned i=0;i<100;i++){source_rgb[0][(90*320+140)*3]^=1;top_stage_cached(source_rgb[0],physical[0][0],1,0);}
 double cached_ms=1000.0*(clock()-begin)/CLOCKS_PER_SEC;
 printf("PASS full=%u partial=%u reused=%u saved_rows=%u host_full_ms=%.3f host_cached_ms=%.3f\n",full,partial,reused,saved,full_ms,cached_ms);
 return 0;
}
'''
  work=ROOT/'work';work.mkdir(exist_ok=True)
  with tempfile.TemporaryDirectory(prefix='present_cache_',dir=work) as folder:
   temp=Path(folder);test=temp/'test.c';test.write_text(program,encoding='utf-8')
   exe=temp/('test.exe' if os.name=='nt' else 'test');path,env,msvc=cc
   args=([path,'/nologo','/std:c11','/utf-8','/O2','/MT','/I'+str(source),str(test),'/Fe:'+str(exe)] if msvc else [path,'-O2','-std=c11','-I'+str(source),str(test),'-o',str(exe)])
   result=subprocess.run(args,cwd=temp,env=env,capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
   result=subprocess.run([str(exe)],cwd=temp,env=env,capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
   print(result.stdout.strip())
if __name__=='__main__':unittest.main()
