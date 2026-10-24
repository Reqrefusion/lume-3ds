"""Execute real GPU lifecycle/presentation code with deterministic DMA mocks."""
from pathlib import Path
import os,subprocess,tempfile,unittest
from test_fence_flow import ROOT,compiler,block
class GpuLifecycleTests(unittest.TestCase):
 def test_failures_reopen_and_buffers(self):
  cc=compiler()
  if not cc:self.skipTest('Native C compiler unavailable')
  source=ROOT/'source';text=(ROOT/'source/engine/platform/3ds/video_present.c').read_text(encoding='utf-8')
  program=r"""
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "engine/platform/3ds/video_present.h"
#include "engine/platform/3ds/video_cache.h"
#define LUME_MEDIA_TOP 13
#define LUME_MEDIA_BOTTOM 226
typedef uint64_t u64;typedef uint32_t u32;
#define R_FAILED(x) ((x)<0)
#define R_SUCCEEDED(x) ((x)>=0)
#define GX_TRANSFER_FLIP_VERT(x) 0
#define GX_TRANSFER_OUT_TILED(x) 0
#define GX_TRANSFER_RAW_COPY(x) 0
#define GX_TRANSFER_IN_FORMAT(x) 0
#define GX_TRANSFER_OUT_FORMAT(x) 0
#define GX_TRANSFER_SCALING(x) 0
#define GX_BUFFER_DIM(x,y) 0
#define CHECK(x) do{if(!(x)){printf("FAIL %d\n",__LINE__);return 1;}}while(0)
static struct {void *data;} textures[2];
typedef struct {struct {void *colorBuf;} framebuf_type;} unused_type;
typedef struct {struct {void *colorBuf;} frameBuf;} Target;
static Target target_storage[2],*targets[2];
static bool c3d_ready,c2d_ready;static LumeVideoCache cache;
static LumeGpuVideoStats stats;
static unsigned init_count,renders,copies,uploads,deletes;static int dma_failure;static bool render_ok=true;
static u64 svcGetSystemTick(void){static u64 t;return ++t;}
static void lume_gpu_video_invalidate(void){lume_video_cache_reset(&cache);}
static void C3D_RenderTargetDelete(Target *t){(void)t;deletes++;}
static void C3D_TexDelete(void *t){(void)t;deletes++;}
static void C2D_Fini(void){}
static void C3D_Fini(void){}
static int lume_dma_flush(void *p,unsigned bytes){(void)p;(void)bytes;return dma_failure==1?-1:0;}
static int lume_dma_invalidate(void *p,unsigned bytes){(void)p;(void)bytes;return dma_failure==2?-1:0;}
static void C3D_SyncDisplayTransfer(void *input,unsigned a,u32 *out,unsigned b,unsigned flags){
 (void)a;(void)b;(void)flags;copies++;memset(out,input==target_storage?11:22,288000);
}
static bool initialize(void){
 if(stats.attempted)return stats.ready;
 init_count++;stats.attempted=stats.ready=true;
 for(unsigned i=0;i<2;i++){targets[i]=target_storage+i;targets[i]->frameBuf.colorBuf=target_storage+i;textures[i].data=target_storage+i;}
 c3d_ready=c2d_ready=true;return true;
}
static void upload(unsigned eye,const uint8_t *rgb,unsigned first,unsigned height){(void)eye;(void)rgb;(void)first;(void)height;uploads++;}
static bool render(unsigned height,bool stereo){(void)height;(void)stereo;renders++;return render_ok;}
"""
  for marker in ['void lume_gpu_video_close(', 'static bool fail(', 'static bool transfer(', 'bool lume_gpu_video_present(']:program+='\n'+block(text,marker)
  program+=r"""
static uint8_t left[230400],right[230400],fb[2][2][288000];
int main(void){
 CHECK(!lume_gpu_video_present(0,NULL,right,fb[0][0],fb[1][0]));CHECK(init_count==0);
 CHECK(lume_gpu_video_present(1,left,right,fb[0][0],fb[1][0]));CHECK(renders==1&&copies==2&&uploads==2);
 CHECK(fb[0][0][0]==11&&fb[1][0][0]==22);
 CHECK(lume_gpu_video_present(1,left,right,fb[0][1],fb[1][1]));CHECK(renders==1&&copies==4);
 CHECK(lume_gpu_video_present(1,left,right,fb[0][0],fb[1][0]));CHECK(copies==4);
 lume_gpu_video_close();CHECK(!stats.attempted&&!stats.ready&&deletes==4);
 CHECK(lume_gpu_video_present(2,left,NULL,fb[0][0],NULL));CHECK(init_count==2&&stats.ready);
 for(int failure=1;failure<=2;failure++){
  dma_failure=failure;unsigned before=copies;
  CHECK(!lume_gpu_video_present(3+failure,left,right,fb[0][0],fb[1][0]));
  CHECK(!stats.ready&&stats.attempted&&!strcmp(stats.state,"transfer_cache_failed"));
  CHECK(copies==before+(failure==2)); /* Flush failure must prevent DMA. */
  unsigned attempts=init_count;
  CHECK(!lume_gpu_video_present(10,left,right,fb[0][0],fb[1][0]));CHECK(init_count==attempts);
  dma_failure=0;lume_gpu_video_close();
  CHECK(lume_gpu_video_present(20,left,right,fb[0][0],fb[1][0]));
 }
 render_ok=false;CHECK(!lume_gpu_video_present(30,left,right,fb[0][0],fb[1][0]));
 CHECK(!strcmp(stats.state,"render_failed")&&!stats.ready);lume_gpu_video_close();
 puts("PASS actual GPU control flow: separate eyes, four buffers, flush/invalidate failures, latched fallback, explicit reopen, NULL mono");return 0;
}
"""
  work=ROOT/'work';work.mkdir(exist_ok=True)
  with tempfile.TemporaryDirectory(prefix='gpu_lifecycle_',dir=work) as folder:
   temp=Path(folder);test=temp/'test.c';test.write_text(program,encoding='utf-8')
   exe=temp/('test.exe' if os.name=='nt' else 'test');path,env,msvc=cc
   args=([path,'/nologo','/std:c11','/utf-8','/O2','/MT','/I'+str(source),str(test),'/Fe:'+str(exe)] if msvc else [path,'-O2','-std=c11','-I'+str(source),str(test),'-o',str(exe)])
   result=subprocess.run(args,cwd=temp,env=env,capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
   result=subprocess.run([str(exe)],cwd=temp,env=env,capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
   print(result.stdout.strip())
if __name__=='__main__':unittest.main()
