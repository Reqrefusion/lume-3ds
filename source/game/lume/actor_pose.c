#include "storage.h"
#ifndef LUME_MEDIA_TOP
#define LUME_MEDIA_TOP 13
#define LUME_MEDIA_BOTTOM 226
#endif
#include "actor_pose.h"
#include "../../engine/render/blit.h"
#include "../../engine/render/depth_anchor.h"
#include <stdio.h>
#include <string.h>
const int lume_actor_feet[7][2] = {{120, 216}, {204, 158}, {211, 167},
                                   {148, 172}, {92, 170},  {113, 185},
                                   {206, 210}};
const float lume_actor_heights[7] = {55.f, 45.f, 46.f, 61.f, 79.f, 67.f, 48.f};
typedef struct {
  int x, y;
  unsigned w, h;
  bool ready;
  char detail[80];
  uint8_t rgba[96 * 96 * 4];
} Pose;
static Pose poses[6];
static bool capture_pose(int scene,int x,int y,unsigned w,unsigned h,
                         const uint8_t *rgba) {
  if(scene<0||scene>=6||!rgba||!w||!h||w>320||h>240)return false;
  unsigned left=0,top=0,right=w,bottom=h;
  if(w>96||h>96) {
    /* A padded terminal crop can be larger than the pose cache even when
       its visible actor fits. Trim transparent padding, never rescale or
       substitute an idle pose for the source landing frame. */
    left=w;top=h;right=bottom=0;
    for(unsigned yy=0;yy<h;yy++)for(unsigned xx=0;xx<w;xx++)
      if(rgba[(yy*w+xx)*4+3]) {
        if(xx<left)left=xx;
        if(yy<top)top=yy;
        if(xx+1>right)right=xx+1;
        if(yy+1>bottom)bottom=yy+1;
      }
    if(left>=right||top>=bottom||right-left>96||bottom-top>96)return false;
  }
  Pose *p=&poses[scene];p->x=x+(int)left;p->y=y+(int)top;
  p->w=right-left;p->h=bottom-top;
  for(unsigned yy=0;yy<p->h;yy++)
    memmove(p->rgba+yy*p->w*4,rgba+((top+yy)*w+left)*4,p->w*4);
  p->ready=true;p->detail[0]=0;return true;
}
void lume_actor_capture(int scene,int x,int y,unsigned w,unsigned h,const uint8_t *rgba) {
  (void)capture_pose(scene,x,y,w,h,rgba);
}
void lume_actor_restore(int i) {
    if (i < 0 || i >= 6) return;
    poses[i].ready = false;
    char path[80];
    snprintf(path, sizeof(path), LUME_DATA_FILE("actor_idle_%d.bin"), i);
    FILE *f = fopen(path, "rb");
    if (!f)
      return;
    uint8_t hdr[16];
    uint16_t w = 0, h = 0;
    int16_t x = 0, y = 0;
    if (fread(hdr, 1, 16, f) == 16 && !memcmp(hdr, "IDL1", 4)) {
      memcpy(&w, hdr + 4, 2);
      memcpy(&h, hdr + 6, 2);
      memcpy(&x, hdr + 8, 2);
      memcpy(&y, hdr + 10, 2);
      if (w && w <= 96 && h && h <= 96 &&
          fread(poses[i].rgba, 1, w * h * 4, f) == w * h * 4) {
        poses[i].w = w;
        poses[i].h = h;
        poses[i].x = x;
        poses[i].y = y;
        poses[i].ready = true;
        snprintf(poses[i].detail,sizeof poses[i].detail,"actor_idle_%d",i);
      }
    }
    fclose(f);
}
void lume_actor_init(void) {
  for (int i = 0; i < 6; i++) lume_actor_restore(i);
}
bool lume_actor_ground(int scene,int x,int y,int *foot_x,int *foot_y) {
  if(scene<0||scene>=6||!poses[scene].ready)return false;
  const Pose *p=&poses[scene];int fx,fy;
  if(!lume_depth_crop_foot(p->rgba,p->w,p->h,
       p->x+x-lume_actor_feet[scene][0],p->y+y-lume_actor_feet[scene][1],&fx,&fy))return false;
  *foot_x=fx;*foot_y=fy;return true;
}
bool lume_actor_idle(uint8_t *rgb, int scene, int x, int y, int eye,int quarter_pixels) {
  if (scene < 0 || scene >= 6 || !poses[scene].ready)
    return false;
  const Pose *p = &poses[scene];
  lume_blit_rgba_parallax(rgb, p->rgba, p->w, p->h,
                 p->x + x - lume_actor_feet[scene][0],
                 p->y + y - lume_actor_feet[scene][1],eye,quarter_pixels);
  return true;
}

const char *lume_actor_detail(int scene) {
  if(scene<0||scene>=6||!poses[scene].ready||!poses[scene].detail[0])return NULL;
  return poses[scene].detail;
}
bool lume_actor_capture_named_checked(int scene,int x,int y,unsigned w,unsigned h,
    const uint8_t *rgba,const char *clip) {
  if(!capture_pose(scene,x,y,w,h,rgba))return false;
  if(clip&&*clip)snprintf(poses[scene].detail,sizeof poses[scene].detail,"actor_%s",clip);
  return true;
}
void lume_actor_capture_named(int scene,int x,int y,unsigned w,unsigned h,
    const uint8_t *rgba,const char *clip) {
  (void)lume_actor_capture_named_checked(scene,x,y,w,h,rgba,clip);
}
