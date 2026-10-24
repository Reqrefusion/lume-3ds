/* An independent application: generated artwork, no game media or rules.
   START exits. The 3D slider controls the horizontal stereo displacement. */
#include <3ds.h>
#include <stdint.h>
#include <string.h>
#include "engine/core/jobs_state.h"
#include "engine/render/blit.h"
#include "engine/platform/3ds/video_present.h"
#include "engine/platform/3ds/frame_flush.h"
static uint8_t eyes[2][320*240*3],bottom[320*240*3];
static void draw(unsigned frame,unsigned shift){
 for(unsigned eye=0;eye<2;eye++)for(unsigned y=0;y<240;y++)for(unsigned x=0;x<320;x++) {
  unsigned sx=(x+(eye?shift:0))%320;uint8_t *p=eyes[eye]+(y*320+x)*3;
  bool square=sx>(frame%240)&&sx<(frame%240)+60&&y>90&&y<150;
  p[0]=square?255:(uint8_t)(sx*255/319);p[1]=square?220:(uint8_t)(y*255/239);p[2]=square?40:90;
 }
}
int main(void){
 gfxInitDefault();gfxSet3D(true);
 lume_runtime_set_asset_directory("romfs:");lume_runtime_set_flags_directory(NULL);
 unsigned serial=0;uint64_t last=0;
 while(aptMainLoop()) {
  hidScanInput();if(hidKeysDown()&KEY_START)break;
  uint64_t now=osGetTime();
  if(now-last>=40) {
   last=now;unsigned shift=(unsigned)(osGet3DSliderState()*8);draw(serial++,shift);
   uint8_t *l=gfxGetFramebuffer(GFX_TOP,GFX_LEFT,NULL,NULL),*r=gfxGetFramebuffer(GFX_TOP,GFX_RIGHT,NULL,NULL),*b=gfxGetFramebuffer(GFX_BOTTOM,GFX_LEFT,NULL,NULL);
   bool gpu=lume_gpu_video_present(serial,eyes[0],shift?eyes[1]:eyes[0],l,r);
   if(gpu){lume_present_bottom(bottom,b);lume_flush_framebuffers(LUME_BOTTOM,b,l,r);}
   else {lume_gpu_video_invalidate();lume_present_views(eyes[0],shift?eyes[1]:eyes[0],bottom,b,l,r);lume_flush_framebuffers(LUME_BOTTOM|LUME_TOP_LEFT|LUME_TOP_RIGHT,b,l,r);}
   gfxSwapBuffers();
  }
  gspWaitForVBlank();
 }
 lume_gpu_video_close();gfxExit();return 0;
}
