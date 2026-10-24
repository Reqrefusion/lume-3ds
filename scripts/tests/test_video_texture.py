"""Native checks for the GPU upload layout, crop and filtering guards."""
from pathlib import Path
import os,subprocess,tempfile,unittest
from test_fence_flow import ROOT,compiler
class VideoTextureTests(unittest.TestCase):
 def test_texture_layout_and_guards(self):
  cc=compiler()
  if not cc:self.skipTest('Native C compiler unavailable')
  program=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "engine/platform/3ds/video_texture.h"
static inline void lume_video_texture_pack_v5(uint32_t *out,const uint8_t *rgb,unsigned first,unsigned height) {
 unsigned columns[321];
 for(unsigned x=0;x<=320;x++)columns[x]=(x/8)*64+lume_video_morton(x&7,0);
 for(unsigned y=0;y<=height;y++) {
  unsigned sy=first+(y<height?y:height-1);
  size_t row=(size_t)(y/8)*512*8+lume_video_morton(0,y&7);
  for(unsigned x=0;x<=320;x++) {
   const uint8_t *p=rgb+((size_t)sy*320+(x<320?x:319))*3;
   out[row+columns[x]]=255u|(uint32_t)p[2]<<8|(uint32_t)p[1]<<16|(uint32_t)p[0]<<24;
  }
 }
}

#include "engine/platform/3ds/video_cache.h"
#define CHECK(x) do{if(!(x)){printf("FAIL %d\n",__LINE__);return 1;}}while(0)
static uint32_t storage[512*256+2],expected[512*256];
static uint8_t rgb[320*240*3],original[320*240*3];
static unsigned index_ref(unsigned x,unsigned y){
 unsigned z=0;for(unsigned b=0;b<3;b++){z|=((x>>b)&1)<<(2*b);z|=((y>>b)&1)<<(2*b+1);}
 return ((y/8)*64+x/8)*64+z;
}
int main(void){
 LumeVideoCache cache={0};int l,r,buffers[4];
 CHECK(!lume_video_cache_matches(&cache,0,&l,&r));
 lume_video_cache_commit(&cache,0,&l,&r);
 CHECK(lume_video_cache_matches(&cache,0,&l,&r));
 CHECK(!lume_video_cache_matches(&cache,1,&l,&r));
 CHECK(!lume_video_cache_matches(&cache,0,&l,&l));
 for(unsigned eye=0;eye<2;eye++)for(unsigned i=0;i<2;i++){
  CHECK(!lume_video_buffer_matches(&cache,eye,&buffers[eye*2+i]));
  lume_video_buffer_commit(&cache,eye,&buffers[eye*2+i]);
 }
 for(unsigned eye=0;eye<2;eye++)for(unsigned i=0;i<2;i++)CHECK(lume_video_buffer_matches(&cache,eye,&buffers[eye*2+i]));
 CHECK(!lume_video_buffer_matches(&cache,1,&buffers[0]));
 lume_video_cache_commit(&cache,1,&l,&r);
 CHECK(!lume_video_buffer_matches(&cache,0,&buffers[0]));
 lume_video_buffer_commit(&cache,0,&buffers[0]);
 lume_video_cache_commit(&cache,0,&l,&l);
 CHECK(!lume_video_buffer_matches(&cache,0,&buffers[0]));
 lume_video_buffer_commit(&cache,0,&buffers[0]);
 lume_video_cache_reset(&cache);
 CHECK(!lume_video_cache_matches(&cache,0,&l,&l));
 CHECK(!lume_video_buffer_matches(&cache,0,&buffers[0]));

 for(unsigned i=0;i<sizeof rgb;i++)rgb[i]=(uint8_t)(i*37+i/971);
 memcpy(original,rgb,sizeof rgb);
 for(unsigned profile=0;profile<3;profile++){
  unsigned height=profile==2?240:213+profile,first=profile==2?0:13;
  for(unsigned i=0;i<512*256+2;i++)storage[i]=0x91379bad;
  for(unsigned i=0;i<512*256;i++)expected[i]=0x91379bad;
  lume_video_texture_pack(storage+1,rgb,first,height);
  for(unsigned y=0;y<=height;y++)for(unsigned x=0;x<=320;x++){
   unsigned sy=first+(y<height?y:height-1),sx=x<320?x:319;
   const uint8_t *p=rgb+(sy*320+sx)*3;
   expected[index_ref(x,y)]=255u|((uint32_t)p[2]<<8)|((uint32_t)p[1]<<16)|((uint32_t)p[0]<<24);
  }
  CHECK(storage[0]==0x91379bad&&storage[512*256+1]==0x91379bad);
  CHECK(!memcmp(storage+1,expected,sizeof expected));
  CHECK(!memcmp(rgb,original,sizeof rgb));
 }
 clock_t begin=clock();
 for(unsigned i=0;i<600;i++){rgb[i%sizeof rgb]^=1;lume_video_texture_pack_v5(storage+1,rgb,13,213);}
 double old_ms=1000.0*(clock()-begin)/CLOCKS_PER_SEC;
 begin=clock();
 for(unsigned i=0;i<600;i++){rgb[i%sizeof rgb]^=1;lume_video_texture_pack(storage+1,rgb,13,213);}
 double new_ms=1000.0*(clock()-begin)/CLOCKS_PER_SEC;
 printf("Texture host diagnostic: v5=%.3f ms tile=%.3f ms (600 frames; not ARM FPS)\n",old_ms,new_ms);
 puts("PASS GPU texture: both crops, all texels, channel order, edge guards, bounds");return 0;
}
'''
  work=ROOT/'work';work.mkdir(exist_ok=True)
  with tempfile.TemporaryDirectory(prefix='video_texture_',dir=work) as folder:
   temp=Path(folder);test=temp/'test.c';test.write_text(program,encoding='utf-8')
   exe=temp/('test.exe' if os.name=='nt' else 'test');path,env,msvc=cc
   source=ROOT/'source'
   args=([path,'/nologo','/std:c11','/utf-8','/O2','/MT','/I'+str(source),str(test),'/Fe:'+str(exe)] if msvc else [path,'-O2','-std=c11','-I'+str(source),str(test),'-o',str(exe)])
   result=subprocess.run(args,cwd=temp,env=env,capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
   result=subprocess.run([str(exe)],cwd=temp,env=env,capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
   print(result.stdout.strip())
if __name__=='__main__':unittest.main()
