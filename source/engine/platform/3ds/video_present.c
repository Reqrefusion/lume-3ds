/* GPU 2D staging only: decoded eyes and all game/AVM1 decisions stay on CPU.
   Off-screen targets are NEVER linked to LCD outputs. The existing main loop
   owns swaps, bottom-screen UI and VBlank. RGB888 output is preserved. */
#include "engine/platform/3ds/video_present.h"
#include "engine/platform/3ds/video_texture.h"
#include "engine/core/layout.h"
#include "engine/platform/3ds/video_cache.h"
#include "engine/platform/3ds/dma_cache.h"
#include "engine/core/jobs_state.h"
#include <3ds.h>
#include <citro2d.h>
#include <string.h>
#include <stdlib.h>
static LumeVideoCache cache;
void lume_gpu_video_invalidate(void){lume_video_cache_reset(&cache);}
static C3D_Tex textures[2];
static C3D_RenderTarget *targets[2];
static bool c3d_ready,c2d_ready;
static LumeGpuVideoStats stats={.state="not_attempted"};
const LumeGpuVideoStats *lume_gpu_video_stats(void){return &stats;}
void lume_gpu_video_close(void) {
 lume_gpu_video_invalidate();
 for(unsigned i=0;i<2;i++) {
  if(targets[i]){C3D_RenderTargetDelete(targets[i]);targets[i]=NULL;}
  if(textures[i].data){C3D_TexDelete(&textures[i]);memset(&textures[i],0,sizeof textures[i]);}
 }
 if(c2d_ready){C2D_Fini();c2d_ready=false;}
 if(c3d_ready){C3D_Fini();c3d_ready=false;}
 stats.ready=false;stats.attempted=false;stats.state="closed";
}
static bool fail(const char *reason) {
 lume_gpu_video_close();stats.attempted=true;stats.failures++;stats.state=reason;return false;
}
static void upload(unsigned eye,const uint8_t *rgb,unsigned first,unsigned height) {
 lume_video_texture_pack(textures[eye].data,rgb,first,height);
 C3D_TexFlush(&textures[eye]);
}
static bool render(unsigned height,bool stereo) {
 if(!C3D_FrameBegin(C3D_FRAME_NONBLOCK))return false;
 C2D_Prepare();C3D_DepthTest(false,GPU_ALWAYS,GPU_WRITE_COLOR);
 Tex3DS_SubTexture sub={.width=320,.height=height,.left=0,.top=1,
  .right=320.0f/512,.bottom=1.0f-height/256.0f};
 for(unsigned eye=0;eye<(stereo?2u:1u);eye++) {
  C2D_TargetClear(targets[eye],C2D_Color32(0,0,0,255));
  C2D_SceneBegin(targets[eye]);C2D_SceneSize(240,400,true);
  C2D_Image image={&textures[eye],&sub};
  if(!C2D_DrawImageAt(image,20,0,0,NULL,360.0f/320,240.0f/height)) {
   C3D_FrameEnd(GX_CMDLIST_FLUSH);return false;
  }
 }
 C2D_Flush();
 /* Textures and C2D vertices are flushed explicitly. Do not clean the
    entire linear heap (including unrelated audio buffers) every frame. */
 C3D_FrameEnd(GX_CMDLIST_FLUSH);return true;
}
static bool transfer(unsigned eye,uint8_t *fb) {
 /* A previous CPU-rendered frame must be clean before DMA replaces it. */
 if(R_FAILED(lume_dma_flush(fb,400*240*3)))return false;
 unsigned flags=GX_TRANSFER_FLIP_VERT(0)|GX_TRANSFER_OUT_TILED(0)|
  GX_TRANSFER_RAW_COPY(0)|GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|
  GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8)|GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);
 C3D_SyncDisplayTransfer(targets[eye]->frameBuf.colorBuf,GX_BUFFER_DIM(240,400),
  (u32*)fb,GX_BUFFER_DIM(240,400),flags);
 return R_SUCCEEDED(lume_dma_invalidate(fb,400*240*3));
}
static bool calibration(void) {
 uint8_t *source=malloc(320*240*3),*left=linearAlloc(400*240*3),*right=linearAlloc(400*240*3);
 if(!source||!left||!right){free(source);if(left)linearFree(left);if(right)linearFree(right);return false;}
 for(unsigned y=0;y<240;y++)for(unsigned x=0;x<320;x++) {
  uint8_t *p=source+(y*320+x)*3;
  p[0]=((x<160&&y<120)||(x>=160&&y>=120))?255:0;
  p[1]=x>=160?255:0;p[2]=y>=120?255:0;
 }
 upload(0,source,0,240);
 for(unsigned i=0;i<320*240;i++){uint8_t r=source[i*3];source[i*3]=source[i*3+2];source[i*3+2]=r;}
 upload(1,source,0,240);
 /* Test the transport independently of even a one-row application crop. */
 bool ok=render(240,true);
 if(ok)ok=transfer(0,left)&&transfer(1,right);
 const unsigned xs[]={100,300,100,300},ys[]={60,60,180,180};
 const uint8_t colours[4][3]={{255,0,0},{0,255,0},{0,0,255},{255,255,255}};
 for(unsigned eye=0;ok&&eye<2;eye++)for(unsigned i=0;i<4;i++)for(unsigned c=0;c<3;c++) {
  const uint8_t *fb=eye?right:left;
  unsigned expected=colours[i][eye?c:2-c];
  int delta=(int)fb[(xs[i]*240+239-ys[i])*3+c]-(int)expected;
  if(delta < -3 || delta > 3)ok=false;
 }
 if(ok)for(unsigned y=0;y<240;y++)for(unsigned c=0;c<3;c++)
  if(left[(5*240+239-y)*3+c]||right[(395*240+239-y)*3+c])ok=false;
 free(source);linearFree(left);linearFree(right);return ok;
}
static bool initialize(void) {
 if(stats.attempted)return stats.ready;
 stats.attempted=true;
 if(lume_runtime_flag("compat_gpu_video.flag")){stats.state="compat_disabled";return false;}
 if(!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE))return fail("c3d_init_failed");
 c3d_ready=true;
 if(!C2D_Init(32))return fail("c2d_init_failed");
 c2d_ready=true;
 for(unsigned i=0;i<2;i++) {
  if(!C3D_TexInit(&textures[i],512,256,GPU_RGBA8))return fail("texture_alloc_failed");
  memset(textures[i].data,0,textures[i].size);
  C3D_TexSetFilter(&textures[i],GPU_LINEAR,GPU_LINEAR);
  C3D_TexSetWrap(&textures[i],GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);
  targets[i]=C3D_RenderTargetCreate(240,400,GPU_RB_RGBA8,-1);
  if(!targets[i])return fail("target_alloc_failed");
 }
 if(!calibration())return fail("calibration_failed");
 stats.ready=true;stats.state="ready_calibrated";return true;
}
bool lume_gpu_video_present(unsigned serial,const uint8_t *left,const uint8_t *right,uint8_t *fb_left,uint8_t *fb_right) {
 if(!left||!fb_left)return false;
 if(!right)right=left;
 if(!fb_right)fb_right=fb_left;
 if(!initialize())return false;
 bool stereo=left!=right;
 if(!lume_video_cache_matches(&cache,serial,left,right)) {
  unsigned height=LUME_MEDIA_BOTTOM-LUME_MEDIA_TOP;
  if(LUME_MEDIA_BOTTOM<240)for(unsigned i=LUME_MEDIA_BOTTOM*320*3;i<(LUME_MEDIA_BOTTOM+1)*320*3;i++)
   if(left[i]||right[i]){height++;break;}
  u64 start=svcGetSystemTick();
  upload(0,left,LUME_MEDIA_TOP,height);if(stereo)upload(1,right,LUME_MEDIA_TOP,height);
  u64 uploaded=svcGetSystemTick();
  if(!render(height,stereo))return fail("render_failed");
  u64 rendered=svcGetSystemTick();
  stats.frames++;stats.upload_ticks+=uploaded-start;stats.render_ticks+=rendered-uploaded;
  /* The first synchronous copy drains this render's GPU commands. */
  if(!transfer(0,fb_left))return fail("transfer_cache_failed");
  stats.transfers++;
  stats.copy_ticks+=svcGetSystemTick()-rendered;
  lume_video_cache_commit(&cache,serial,left,right);
  lume_video_buffer_commit(&cache,0,fb_left);
 }else stats.reused++;
 u64 copy_start=svcGetSystemTick();
 if(!lume_video_buffer_matches(&cache,0,fb_left)) {
  if(!transfer(0,fb_left))return fail("transfer_cache_failed");
  stats.transfers++;lume_video_buffer_commit(&cache,0,fb_left);
 }
 if(fb_right!=fb_left&&!lume_video_buffer_matches(&cache,1,fb_right)) {
  if(!transfer(stereo?1:0,fb_right))return fail("transfer_cache_failed");
  stats.transfers++;lume_video_buffer_commit(&cache,1,fb_right);
 }
 stats.copy_ticks+=svcGetSystemTick()-copy_start;
 return true;
}
