#ifndef LUME_DEPTH_ANCHOR_H
#define LUME_DEPTH_ANCHOR_H
#include <stdbool.h>
#include <stdint.h>
/* Correspondence fields store quarter-pixel RIGHT-to-LEFT offsets, not
   metric distance. An overlay anchored in the left image must invert that
   correspondence before using the same offset in the right image. */
typedef struct {
  int filtered, x, y;
  unsigned source, background, frame;
  bool valid;
} LumeDepthAnchor;
static inline int lume_depth_clamp(int x,int low,int high) {
  return x<low?low:x>high?high:x;
}
static inline int lume_depth_ground(const int8_t *map,int x,int y) {
  if(!map)return 4; /* Retain the former one-pixel plane without valid data. */
  int values[15],n=0;
  for(int dy=0;dy<3;dy++)for(int dx=-4;dx<=4;dx+=2) {
    int lx=lume_depth_clamp(x+dx,0,319);
    int yy=lume_depth_clamp(y+dy,13,225),rx=lx;
    for(int k=0;k<3;k++) {
      int q=map[yy*320+rx];
      rx=lume_depth_clamp((lx*4-q+2)/4,0,319);
    }
    int value=map[yy*320+rx],i=n++;
    while(i && values[i-1]>value){values[i]=values[i-1];i--;}
    values[i]=value;
  }
  return values[7]; /* Ignore an isolated edge or fence post under one foot. */
}
static inline int lume_depth_follow(LumeDepthAnchor *a,int target,int x,int y,
                                    unsigned source,unsigned bg,unsigned frame) {
  bool snap=!a->valid||source!=a->source||frame<a->frame||
    (bg>a->background?bg-a->background:a->background-bg)>4||
    x-a->x>48||a->x-x>48||y-a->y>32||a->y-y>32;
  if(snap)a->filtered=target*256;
  else if(frame!=a->frame) {
    int step=(target*256-a->filtered)/4;
    a->filtered+=lume_depth_clamp(step,-8*256,8*256);
  }
  a->source=source;a->background=bg;a->frame=frame;a->x=x;a->y=y;a->valid=true;
  return a->filtered>=0?(a->filtered+128)/256:-((-a->filtered+128)/256);
}
/* A small LAN crop is the authored actor layer. Find its opaque foot edge,
   ignoring transparent padding and faint dissolve/shadow pixels. Full-screen
   photographic dissolves and large mixed layers are not one actor plane. */
static inline bool lume_depth_actor_crop(unsigned w,unsigned h) {
  return w&&h&&w<=128&&h<=128;
}
static inline bool lume_depth_crop_foot(const uint8_t *rgba,unsigned w,unsigned h,
                                        int x,int y,int *foot_x,int *foot_y) {
  if(!rgba||!lume_depth_actor_crop(w,h))return false;
  for(int row=(int)h-1;row>=0;row--) {
    int sum=0,count=0;
    for(unsigned col=0;col<w;col++)if(rgba[((unsigned)row*w+col)*4+3]>=96) {
      sum+=(int)col;count++;
    }
    if(count) {
      *foot_x=x+sum/count;*foot_y=y+row;
      return *foot_x>=0&&*foot_x<320&&*foot_y>=13&&*foot_y<226;
    }
  }
  return false;
}
#endif
