"""Render the actual parallax path; a full-frame dissolve must cover both edges."""
from pathlib import Path
import os,subprocess,tempfile,unittest
from test_fence_flow import ROOT,compiler,block

class TransitionStereoTests(unittest.TestCase):
 def test_photographic_layer_edges_and_actor_depth(self):
  cc=compiler()
  if not cc:self.skipTest('Native C compiler unavailable')
  source=ROOT/'source'
  main=(source/'main.c').read_text(encoding='utf-8-sig')
  blit=(source/'engine/render/blit.c').read_text(encoding='utf-8-sig')
  program=r'''
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "engine/render/depth_anchor.h"
static struct {unsigned width,height;int x,y;uint8_t *rgba;} animation;
static bool stereo_enabled=true;
static LumeDepthAnchor clip_depth_anchor;
static unsigned strength=256;
static unsigned lume_stereo_strength(void){return strength;}
static void lume_blend_pixel(uint8_t *d,const uint8_t *s,unsigned a){
 for(unsigned c=0;c<3;c++)d[c]=(uint8_t)((s[c]*a+d[c]*(255-a)+127)/255);
}
'''+block(blit,'void lume_blit_rgba(')+'\n'+block(blit,'void lume_blit_rgba_parallax(')+'\n'+block(main,'static int clip_actor_parallax(')+r'''
#define CHECK(x) do {if(!(x)){printf("FAIL line %d\n",__LINE__);return 1;}}while(0)
static uint8_t rgba[320*240*4],left[320*240*3],right[320*240*3];
static int8_t map[320*240];
int main(void){
 memset(map,8,sizeof map);
 for(unsigned i=0;i<320*240;i++){rgba[i*4]=231;rgba[i*4+1]=17;rgba[i*4+2]=99;rgba[i*4+3]=255;}
 animation.rgba=rgba;animation.width=320;animation.height=214;animation.x=0;animation.y=13;
 for(unsigned value=0;value<=256;value+=128){
  strength=value;memset(left,42,sizeof left);memset(right,42,sizeof right);
  int plane=clip_actor_parallax(map,0,0,1);CHECK(plane==0);
  lume_blit_rgba_parallax(left,rgba,320,214,0,13,0,plane);
  lume_blit_rgba_parallax(right,rgba,320,214,0,13,2,plane);
  CHECK(!memcmp(left,right,sizeof left));
  CHECK(right[(100*320+319)*3]==231&&right[(100*320)*3]==231);
 }
 CHECK(clip_actor_parallax(NULL,0,0,2)==0);
 animation.width=32;animation.height=32;animation.x=100;animation.y=100;
 CHECK(clip_actor_parallax(map,0,0,3)==8);
 CHECK(clip_actor_parallax(NULL,0,0,4)==4);
 animation.width=129;CHECK(clip_actor_parallax(map,0,0,5)==0);
 animation.width=0;CHECK(clip_actor_parallax(map,0,0,6)==0);
 puts("PASS: full dissolve edges at zero/half/full strength, actor depth and fallback");return 0;
}
'''
  work=ROOT/'work';work.mkdir(exist_ok=True)
  with tempfile.TemporaryDirectory(prefix='transition_test_',dir=work) as folder:
   temp=Path(folder);test=temp/'test.c';test.write_text(program,encoding='utf-8')
   exe=temp/('test.exe' if os.name=='nt' else 'test');path,env,msvc=cc
   args=([path,'/nologo','/std:c11','/utf-8','/MT','/I'+str(source),str(test),'/Fe:'+str(exe)] if msvc else [path,'-std=c11','-I'+str(source),str(test),'-o',str(exe)])
   result=subprocess.run(args,cwd=temp,env=env,capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
   result=subprocess.run([str(exe)],cwd=temp,env=env,capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
   # Negative control: the former one-pixel fallback must fail the rendered
   # edge/eye equality check, rather than merely matching a function name.
   old=program.replace('clip_depth_anchor.valid=false;return 0;',
                       'clip_depth_anchor.valid=false;return 4;',1)
   old=old.replace('CHECK(plane==0);','')
   self.assertNotEqual(old,program)
   test.write_text(old,encoding='utf-8')
   result=subprocess.run(args,cwd=temp,env=env,capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
   result=subprocess.run([str(exe)],cwd=temp,env=env,capture_output=True,text=True)
   self.assertNotEqual(result.returncode,0,'Former edge shift was not detected')
   self.assertIn('FAIL line',result.stdout)
if __name__=='__main__':unittest.main()
