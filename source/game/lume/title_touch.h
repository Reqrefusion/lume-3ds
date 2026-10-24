#include "storage.h"
#ifndef LUME_TITLE_TOUCH_H
#define LUME_TITLE_TOUCH_H
#include "title_menu.h"
#include "../../engine/assets/detail.h"
#include "../../adapters/flash/timeline.h"
#include <string.h>
/* The bottom view owns interaction rectangles; changing the overview or its
   animation clock cannot move a touch target. Rects accompany the source art. */
typedef struct {int x,y,w,h;} LumeTitleRect;
typedef struct {LumeTitleRect r[9];} LumeTitleLayout;
static inline bool lume_title_inside(const LumeTitleRect *r,int x,int y) {
 return x>=r->x&&y>=r->y&&x<r->x+r->w&&y<r->y+r->h;
}
static inline void lume_title_layout(LumeTitleLayout *l) {
 *l=(LumeTitleLayout){{{24,38,272,44},{24,94,272,44},{24,150,272,44},
 {24,76,272,44},{24,136,272,44},{69,117,74,40},{170,117,74,40},
 {127,178,64,29},{8,200,40,40}}};
 FILE *f=fopen(LUME_DATA_FILE("detail/title_touch.layout"),"rb");if(!f)return;
 uint8_t b[76];bool ok=fread(b,1,sizeof b,f)==sizeof b&&!memcmp(b,"LTT1",4);fclose(f);
 LumeTitleLayout v=*l;
 for(unsigned i=0;ok&&i<9;i++){
  int a[4];for(unsigned k=0;k<4;k++)a[k]=b[4+i*8+k*2]|b[5+i*8+k*2]<<8;
  ok=a[0]>=0&&a[1]>=0&&a[2]>0&&a[3]>0&&a[0]+a[2]<=320&&a[1]+a[3]<=240;
  v.r[i]=(LumeTitleRect){a[0],a[1],a[2],a[3]};
 }
 if(ok)*l=v;
}
static inline int lume_title_touch_bottom(LumeTitleMenu *m,const LumeTitleLayout *l,int x,int y) {

 if(m->page==TITLE_CREDITS){if(lume_title_inside(&l->r[7],x,y))m->page=TITLE_MAIN;return TITLE_NONE;}
 if(m->page==TITLE_CONFIRM){
  for(int i=0;i<2;i++)if(lume_title_inside(&l->r[5+i],x,y)){m->confirm=i;m->focused=true;return lume_title_activate(m);}
  return TITLE_NONE;
 }
 for(int item=m->has_save?0:1;item<3;item++){
  unsigned slot=m->has_save?(unsigned)item:(unsigned)(2+item);
  if(lume_title_inside(&l->r[slot],x,y)){m->selection=item;m->focused=true;return lume_title_activate(m);}
 }
 return TITLE_NONE;
}
static inline void lume_title_bottom_name(const LumeTitleMenu *m,char *name,size_t n) {
 if(m->page==TITLE_CREDITS)snprintf(name,n,"title_touch_credits");
 else if(m->page==TITLE_CONFIRM)snprintf(name,n,"title_touch_confirm_%d",m->confirm);
 else snprintf(name,n,"title_touch_%s_%d",m->has_save?"saved":"empty",m->selection);
}
static inline bool lume_title_bottom_draw(LumeDetail *d,const LumeTitleMenu *m,bool muted,uint8_t *out) {
 char name[80];lume_title_bottom_name(m,name,sizeof name);
 int changed=lume_detail_begin(d,name,muted);if(changed<0)return false;

 LumeViewport fixed;lume_view_init(&fixed,0);lume_view_draw(&fixed,d->rgb,800,600,out);return true;
}
static inline bool lume_title_top_base(LumeDetail *d,uint8_t *out) {
 if(lume_detail_begin(d,"title_touch_top",0)<0)return false;
 LumeViewport fixed;lume_view_init(&fixed,0);lume_view_draw(&fixed,d->rgb,800,600,out);return true;
}
/* Keep the padded stage border exactly black. A fractional source edge
   must not leave a coloured seam outside the photographed 213-row viewport. */
static inline void lume_title_stage_bands(uint8_t *rgb) {
 memset(rgb,0,13*320*3);
 memset(rgb+226*320*3,0,14*320*3);
}
/* Existing streams contain the desktop speaker overlay. Only its 20px corner
   is replaced with an authored clean crop; the rest of every frame is reused. */
static inline bool lume_title_top_layer(LumeAnimation *clip,LumeAnimation *corner,unsigned frame,uint8_t *out) {
 if(!lume_animation_frame(clip,frame,NULL,0))return false;
 /* The crop is also the reference for the next LAN delta/reference frame.
    Mask only during blitting; clearing alpha here corrupts future decoding. */
 lume_blit_rgba_clipped(out,clip->rgba,clip->width,clip->height,clip->x,clip->y,
                       20,13,320,33);
 lume_blit_rgba_clipped(out,clip->rgba,clip->width,clip->height,clip->x,clip->y,
                       0,33,320,226);
 if(corner->file)lume_animation_frame(corner,frame,out,0);
 return true;
}
#endif
