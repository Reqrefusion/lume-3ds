#ifndef LUME_VIEWPORT_H
#define LUME_VIEWPORT_H
#include <stdint.h>
#include "engine/core/layout.h"
#include "engine/core/jobs.h"
#include <stdbool.h>
#include <string.h>
/* Independent bottom-screen camera. Coordinates and zoom use 10 fraction
   bits; authored scene rendering and the stereo eyes never receive this crop. */
/* Defaults preserve Lume's authored stage; full-screen adapters override them. */
#ifndef LUME_VIEWPORT_TOP
#define LUME_VIEWPORT_TOP 13
#endif
#ifndef LUME_VIEWPORT_BOTTOM
#define LUME_VIEWPORT_BOTTOM 227
#endif

typedef struct {
 int scale,target,cx,cy;uint64_t tick,repeat;unsigned serial;
 int zoom_from;unsigned zoom_elapsed;
} LumeViewport;
enum {LUME_ZOOM_TRANSITION_MS=200};
static inline bool lume_view_moving(const LumeViewport *v) {
 return v->scale!=v->target;
}
static inline void lume_view_init(LumeViewport *v,uint64_t now) {
 *v=(LumeViewport){.scale=1024,.target=1024,.cx=160*1024,.cy=120*1024,
                   .tick=now,.repeat=now,.zoom_from=1024};
}
/* Film playback uses the authored full frame. Preserve presentation serials
   when centering the bottom camera so buffer caches cannot retain a crop. */
static inline bool lume_view_reset(LumeViewport *v,uint64_t now) {
 bool changed=v->scale!=1024||v->target!=1024||v->cx!=160*1024||v->cy!=120*1024;
 v->scale=v->target=1024;v->cx=160*1024;v->cy=120*1024;
 v->zoom_from=1024;v->zoom_elapsed=0;
 v->tick=now;v->repeat=now+300;
 if(changed)v->serial++;
 return changed;
}
static inline void lume_view_clamp(LumeViewport *v) {
 int hx=160*1024*1024/v->scale,hy=120*1024*1024/v->scale;
 /* Keep the camera inside the adapter's authored vertical stage. */
 int half_height=(LUME_VIEWPORT_BOTTOM-LUME_VIEWPORT_TOP)*512;
 if(hy>half_height)hy=half_height;
 if(v->cx<hx)v->cx=hx;
 if(v->cx>320*1024-hx)v->cx=320*1024-hx;
 if(v->cy<LUME_VIEWPORT_TOP*1024+hy)v->cy=LUME_VIEWPORT_TOP*1024+hy;
 if(v->cy>LUME_VIEWPORT_BOTTOM*1024-hy)v->cy=LUME_VIEWPORT_BOTTOM*1024-hy;
}
static inline bool lume_view_tick_mode(LumeViewport *v,uint64_t now,int direction,bool pressed,int dx,int dy,bool immediate) {
 unsigned ms=now>=v->tick?(unsigned)(now-v->tick>50?50:now-v->tick):0;
 v->tick=now;int old=v->scale,x=v->cx,y=v->cy;
 /* Time-based smoothstep has zero speed at each endpoint. Keep scale in the
    actual camera state so drawing, panning and inverse touch share it. */
 if(lume_view_moving(v)) {
  unsigned elapsed=v->zoom_elapsed+ms;
  if(elapsed>=LUME_ZOOM_TRANSITION_MS) {
   v->scale=v->target;v->zoom_elapsed=LUME_ZOOM_TRANSITION_MS;
  } else {
   v->zoom_elapsed=elapsed;
   int64_t t=(int64_t)elapsed*65536/LUME_ZOOM_TRANSITION_MS;
   int64_t weight=t*t*(3*65536-2*t)/(65536LL*65536);
   int64_t delta=(int64_t)(v->target-v->zoom_from)*weight;
   v->scale=v->zoom_from+(int)((delta+(delta>=0?32768:-32768))/65536);
  }
 }
 if(direction && (pressed || now>=v->repeat)) {
  int previous=v->target;
  v->target+=direction*512;
  if(v->target<1024)v->target=1024;
  if(v->target>4096)v->target=4096;
  if(previous!=v->target) {
   v->zoom_from=v->scale;v->zoom_elapsed=0;
  }
  v->repeat=now+(pressed?300:180);
 }
 /* Horizontal D-pad remains the explicit instant-step control. Releasing it
    must not create another animation toward an already reached endpoint. */
 if(immediate && direction) {
  v->scale=v->target;v->zoom_from=v->target;v->zoom_elapsed=0;
 }
 if(v->scale>1024 && ms) {
  if(dx>24||dx< -24)v->cx+=dx*(int)ms*512/v->scale;
  if(dy>24||dy< -24)v->cy-=dy*(int)ms*512/v->scale;
 }
 lume_view_clamp(v);
 bool changed=old!=v->scale||x!=v->cx||y!=v->cy;
 if(changed)v->serial++;
 return changed;
}
static inline bool lume_view_tick(LumeViewport *v,uint64_t now,int direction,bool pressed,int dx,int dy) {
 return lume_view_tick_mode(v,now,direction,pressed,dx,dy,false);
}
/* Explicit instant step for the horizontal D-pad. */
static inline bool lume_view_fast_tick(LumeViewport *v,uint64_t now,int direction,bool pressed,int dx,int dy) {
 return lume_view_tick_mode(v,now,direction,pressed,dx,dy,true);
}
/* Inverse of the displayed viewport. Only explicitly pinned close/sound
   controls retain screen coordinates. */
static inline void lume_view_touch(const LumeViewport *v,int *x,int *y,bool pinned) {
 if(v->scale==1024 || pinned)return;
 *x=(v->cx+(int64_t)(*x-160)*1024*1024/v->scale)/1024;
 *y=(v->cy+(int64_t)(*y-120)*1024*1024/v->scale)/1024;
 if(*x<0)*x=0;
 if(*x>319)*x=319;
 if(*y<LUME_MEDIA_TOP)*y=LUME_MEDIA_TOP;
 if(*y>(LUME_MEDIA_BOTTOM<240?LUME_MEDIA_BOTTOM:239))*y=LUME_MEDIA_BOTTOM<240?LUME_MEDIA_BOTTOM:239;
}
static inline void lume_view_draw_rows(const LumeViewport *v,const uint8_t *src,unsigned width,unsigned height,uint8_t *out,unsigned first,unsigned last) {
 /* Fill every output row. Keep equal x/y scale so the stage is not stretched. */
 struct {unsigned sx,nx,fx;} columns[320];
 for(int x=0;x<320;x++) {
  int64_t px=(int64_t)v->cx*64+(int64_t)(2*x-319)*32768*1024/v->scale;
  int sxq=(int)(px*width/320-32768);
  if(sxq<0)sxq=0;
  if(sxq>(int)(width-1)*65536)sxq=(width-1)*65536;
  unsigned sx=(unsigned)sxq>>16;
  columns[x].sx=sx;columns[x].nx=sx+1<width?sx+1:sx;
  columns[x].fx=((unsigned)sxq>>8)&255;
 }
 unsigned first_row=0,last_row=height-1;
 if(v->scale>1024) {
  first_row=(LUME_MEDIA_TOP*height+120)/240;last_row=(LUME_MEDIA_BOTTOM*height+120)/240;
  if(last_row>=height)last_row=height-1;
  if(width==320 && height==240) {
   bool extended=false;
   for(unsigned i=LUME_MEDIA_BOTTOM*320*3;i<(LUME_MEDIA_BOTTOM<240?LUME_MEDIA_BOTTOM+1:LUME_MEDIA_BOTTOM)*320*3;i++)if(src[i]){extended=true;break;}
   if(!extended&&LUME_MEDIA_BOTTOM<240)last_row=LUME_MEDIA_BOTTOM-1;
  }
 }
 /* At native source size or above, preserve the source pixel contrast.
    Bilinear enlargement mixes thin ink strokes with paper and makes text
    harder to read. Downsampling still uses the full-precision filter below. */
 if(!lume_view_moving(v) && (uint64_t)width*1024<=(uint64_t)320*v->scale &&
    (uint64_t)height*1024<=(uint64_t)240*v->scale) {
  for(int y=(int)first;y<(int)last;y++) {
   int64_t py=(int64_t)v->cy*64+(int64_t)(2*y-239)*32768*1024/v->scale;
   int row=(int)(py*height/240/65536);
   if(row<(int)first_row)row=(int)first_row;
   if(row>(int)last_row)row=(int)last_row;
   for(unsigned x=0;x<320;x++) {
    unsigned sx=columns[x].fx>=128?columns[x].nx:columns[x].sx;
    memcpy(out+(y*320+x)*3,src+(row*width+sx)*3,3);
   }
  }
  return;
 }
 /* Cache the two neighbouring source rows at full horizontal precision.
    Delaying rounding until the vertical pass keeps every output byte equal
    to the four-sample formula, while reused rows avoid repeated blends. */
 uint16_t horizontal[2][320*3];unsigned tags[2]={~0u,~0u};
 for(int y=(int)first;y<(int)last;y++) {
  int64_t py=(int64_t)v->cy*64+(int64_t)(2*y-239)*32768*1024/v->scale;
  int syq=(int)(py*height/240-32768);
  if(syq<(int)first_row*65536)syq=first_row*65536;
  if(syq>(int)last_row*65536)syq=last_row*65536;
  unsigned sy=(unsigned)syq>>16,fy=((unsigned)syq>>8)&255,ny=sy<last_row?sy+1:sy;
  unsigned needed[2]={sy,ny};
  for(unsigned k=0;k<2;k++) {
   unsigned row=needed[k],slot=row&1;
   if(tags[slot]==row)continue;
   const uint8_t *scan=src+row*width*3;
   for(unsigned x=0;x<320;x++) {
    unsigned fx=columns[x].fx,inv=256-fx;
    const uint8_t *a=scan+columns[x].sx*3,*b=scan+columns[x].nx*3;
    for(unsigned c=0;c<3;c++)horizontal[slot][x*3+c]=a[c]*inv+b[c]*fx;
   }
   tags[slot]=row;
  }
  const uint16_t *a=horizontal[sy&1],*b=horizontal[ny&1];
  uint8_t *dst=out+y*320*3;
  for(unsigned x=0;x<320*3;x++)dst[x]=(uint8_t)((a[x]*(256-fy)+b[x]*fy+32768)>>16);
 }
}
typedef struct {const LumeViewport *view;const uint8_t *source;unsigned width,height;uint8_t *out;} LumeViewJob;
static inline void lume_view_draw_job(void *arg) {
 LumeViewJob *j=arg;lume_view_draw_rows(j->view,j->source,j->width,j->height,j->out,120,240);
}
static inline void lume_view_draw(const LumeViewport *v,const uint8_t *src,unsigned width,unsigned height,uint8_t *out) {
 lume_view_draw_rows(v,src,width,height,out,0,240);
}
static inline void lume_view_draw_parallel(const LumeViewport *v,const uint8_t *src,unsigned width,unsigned height,uint8_t *out,LumeMovieJobs *jobs) {
 LumeViewJob j={v,src,width,height,out};
 if(lume_jobs_start(jobs,lume_view_draw_job,&j)) {
  lume_view_draw_rows(v,src,width,height,out,0,120);lume_jobs_wait(jobs);
 }else lume_view_draw(v,src,width,height,out);
}
/* Preserve low-resolution foreground UI without throwing away an original
   page's detail. The comparison is against the clean overview, not the sharp
   page, so different source antialiasing never masks the whole page. */
static inline void lume_view_foreground(const LumeViewport *v,const uint8_t *base,
                                      const uint8_t *composite,uint8_t *out) {
 if(!memcmp(base,composite,320*240*3))return;
 int columns[320];
 for(int x=0;x<320;x++) {
  int sx=(int)(((int64_t)v->cx*64+(int64_t)(2*x-319)*32768*1024/v->scale)/65536);
  columns[x]=sx<0?0:sx>319?319:sx;
 }
 for(int y=0;y<240;y++) {
  int sy=(int)(((int64_t)v->cy*64+(int64_t)(2*y-239)*32768*1024/v->scale)/65536);
  if(sy<LUME_MEDIA_TOP)sy=LUME_MEDIA_TOP;
  if(sy>(LUME_MEDIA_BOTTOM<240?LUME_MEDIA_BOTTOM:239))sy=LUME_MEDIA_BOTTOM<240?LUME_MEDIA_BOTTOM:239;
  for(int x=0;x<320;x++) {
   unsigned i=(unsigned)(sy*320+columns[x])*3;
   if(memcmp(base+i,composite+i,3))memcpy(out+(y*320+x)*3,composite+i,3);
  }
 }
}
#endif
