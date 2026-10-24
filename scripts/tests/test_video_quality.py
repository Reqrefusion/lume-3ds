"""Source-based roof regression, exact RGB warp parity and deadline protection."""
from pathlib import Path
import os,sys,subprocess,tempfile,unittest
import numpy as np,cv2
from PIL import Image
from test_fence_flow import ROOT,compiler,block
sys.path.insert(0,str(ROOT/'scripts/exe'))
from stereo_resample import refine_edges

class VideoQualityTests(unittest.TestCase):
 def test_roof_edge_matches_reference_without_reversed_samples(self):
  fixtures=Path(__file__).parent/'fixtures'
  left=np.array(Image.open(fixtures/'roof_left.jpg').convert('RGB'))
  right=np.array(Image.open(fixtures/'roof_right.jpg').convert('RGB'))
  old=cv2.calcOpticalFlowFarneback(cv2.cvtColor(right,cv2.COLOR_RGB2GRAY),cv2.cvtColor(left,cv2.COLOR_RGB2GRAY),None,.5,3,19,4,5,1.1,0)[:,:,0]
  old=np.clip(old,-31.75,31.75);old[:13]=0;old[226:]=0
  new=refine_edges(left,right,old)
  yy,xx=np.mgrid[:240,:320].astype(np.float32)
  mask=cv2.dilate(cv2.Canny(cv2.cvtColor(right,cv2.COLOR_RGB2GRAY),45,100),np.ones((7,7),np.uint8))>0
  mask[:13]=False;mask[226:]=False
  def error(flow):
   warped=cv2.remap(left,xx+flow,yy,cv2.INTER_LINEAR,borderMode=cv2.BORDER_REPLICATE)
   return np.mean(np.abs(warped.astype(float)-right)[mask])
  self.assertLess(error(new),error(old)*.5)
  self.assertTrue(np.all(np.diff(xx+new,axis=1)>=0))
  self.assertTrue(np.all(new[:13]==0) and np.all(new[226:]==0))
 def test_readiness_does_not_rebase_video_deadline(self):
  main=(ROOT/'source/main.c').read_text(encoding='utf-8-sig')
  waiting=block(main,'if(!lume_movie_ready(movie))')
  self.assertNotIn('lume_animation_retry_start',waiting)
  self.assertNotIn('clip_kind!=3',waiting)
 def test_native_warp_is_byte_exact(self):
  cc=compiler()
  if not cc:self.skipTest('Native C compiler unavailable')
  source=ROOT/'source'
  waiting=block((source/'main.c').read_text(encoding='utf-8-sig'),'if(!lume_movie_ready(movie))')
  program=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "engine/render/disparity.h"
#include "engine/core/timeline_clock.h"
#include <stdbool.h>
static unsigned clip_kind;static uint64_t clip_start,tick,clip_tick_time;
static void *movie;
static bool lume_movie_ready(void *unused){(void)unused;return false;}
static bool poll_wait(void){
'''+waiting+r'''
 return false;
}
static uint8_t left[230400],actual[230400],expected[230400];
static int8_t depth[76800];
int main(void){
 clip_kind=1;clip_start=10;tick=50;clip_tick_time=20;
 if(!poll_wait()||clip_start!=10||lume_animation_clock_frame(tick,clip_start,25,100,0)!=1)return 2;
 clip_kind=3;clip_start=10;tick=50;clip_tick_time=20;
 if(!poll_wait()||clip_start!=40)return 3;
 uint32_t state=17;
 for(unsigned i=0;i<230400;i++){state=state*1664525u+1013904223u;left[i]=(uint8_t)(state>>24);}
 for(unsigned i=0;i<76800;i++){state=state*1664525u+1013904223u;depth[i]=(int8_t)(state>>24);}
 memset(depth,0,320*13);memset(depth+226*320,0,14*320);
 for(unsigned strength=0;strength<=256;strength+=16){
  lume_puzzle_reproject_strength_rows(left,actual,depth,strength,0,120);
  lume_puzzle_reproject_strength_rows(left,actual,depth,strength,120,240);
  for(unsigned y=0;y<240;y++)for(unsigned x=0;x<320;x++){
   int q=(int)x*1024+depth[y*320+x]*(int)strength;
   if(q<0)q=0;if(q>319*1024)q=319*1024;
   unsigned a=(unsigned)q>>10,t=(unsigned)q&1023,b=a<319?a+1:a;
   for(unsigned c=0;c<3;c++)expected[(y*320+x)*3+c]=(uint8_t)((left[(y*320+a)*3+c]*(1024-t)+left[(y*320+b)*3+c]*t+512)>>10);
  }
  if(memcmp(actual,expected,sizeof actual)){printf("FAIL strength %u\n",strength);return 1;}
 }
 puts("PASS: random RGB, extreme signed depth, 17 slider settings, split rows, flat padding");return 0;
}
'''
  work=ROOT/'work';work.mkdir(exist_ok=True)
  with tempfile.TemporaryDirectory(prefix='video_quality_',dir=work) as folder:
   temp=Path(folder);test=temp/'test.c';test.write_text(program,encoding='utf-8')
   exe=temp/('test.exe' if os.name=='nt' else 'test');path,env,msvc=cc
   args=([path,'/nologo','/std:c11','/utf-8','/O2','/MT','/I'+str(source),str(test),'/Fe:'+str(exe)] if msvc else [path,'-O2','-std=c11','-I'+str(source),str(test),'-o',str(exe)])
   result=subprocess.run(args,cwd=temp,env=env,capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
   result=subprocess.run([str(exe)],cwd=temp,env=env,capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
if __name__=='__main__':unittest.main()
