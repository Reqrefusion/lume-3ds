#ifndef LUME_CLUE_READER_H
#define LUME_CLUE_READER_H
#include "clue_views.h"
#include "../../engine/codecs/pixel_image.h"
#include <stdlib.h>
#include <string.h>
#define LUME_READ_WIDTH 800
#define LUME_READ_HEIGHT 600
#define LUME_READ_BYTES (LUME_READ_WIDTH*LUME_READ_HEIGHT*3)
typedef struct { uint8_t *rgb; int key; bool valid; } LumeClueReader;
static inline bool lume_reader_supported(const LumeGame *g) {
  return g->clue==0 || g->clue==1 || (g->clue>=3 && g->clue<=7);
}
static inline void lume_reader_close(LumeClueReader *r) {
  free(r->rgb);r->rgb=NULL;r->key=-1;r->valid=false;
}
static inline bool lume_reader_load(LumeClueReader *r,const LumeGame *g) {
  if(!lume_reader_supported(g))return false;
  int key=lume_clue_cache_key(g);
  if(r->key==key)return r->valid;
  r->valid=false; r->key=key;
  char variant[32],path[128];const char *name=lume_clue_asset(g,variant,sizeof variant);
  snprintf(path,sizeof path,"romfs:/read_%s.rgb",name);
  FILE *f=fopen(path,"rb");if(!f)return false;
  if(!r->rgb)r->rgb=malloc(LUME_READ_BYTES);
  if(r->rgb)r->valid=lume_pixel_image(f,r->rgb,LUME_READ_WIDTH,LUME_READ_HEIGHT);
  if(fclose(f))r->valid=false;
  return r->valid;
}
static inline unsigned lume_reader_next_zoom(unsigned z) {return z==0?2:z==2?4:0;}
static inline void lume_reader_clamp(int *cx,int *cy,unsigned zoom) {
  int halfx=160/(int)zoom,halfy=107/(int)zoom;
  if(*cx<halfx)*cx=halfx;
  if(*cx>320-halfx)*cx=320-halfx;
  if(*cy<13+halfy)*cy=13+halfy;
  if(*cy>227-halfy)*cy=227-halfy;
}
/* Sample the source-sized page, not the reduced 320-pixel overview.
   Fixed-point bilinear interpolation retains source glyph contours while
   panning; one reusable page bounds memory independently of book count. */
static inline bool lume_reader_draw(const LumeClueReader *r,uint8_t *out,int cx,int cy,unsigned zoom) {
  if(!r->valid || !r->rgb || (zoom!=2 && zoom!=4))return false;
  lume_reader_clamp(&cx,&cy,zoom);
  for(int y=13;y<227;y++){
    int v=cy*640+((2*y-239)*320)/(int)zoom-128;
    if(v<0)v=0;
    if(v>599*256)v=599*256;
    unsigned sy=(unsigned)v>>8,fy=(unsigned)v&255,ny=sy<599?sy+1:sy;
    for(int x=0;x<320;x++){
      int u=cx*640+((2*x-319)*320)/(int)zoom-128;
      if(u<0)u=0;
      if(u>799*256)u=799*256;
      unsigned sx=(unsigned)u>>8,fx=(unsigned)u&255,nx=sx<799?sx+1:sx;
      const uint8_t *a=r->rgb+(sy*800+sx)*3,*b=r->rgb+(sy*800+nx)*3;
      const uint8_t *c=r->rgb+(ny*800+sx)*3,*d=r->rgb+(ny*800+nx)*3;
      for(unsigned k=0;k<3;k++)out[(y*320+x)*3+k]=(uint8_t)(((a[k]*(256-fx)+b[k]*fx)*(256-fy)+(c[k]*(256-fx)+d[k]*fx)*fy+32768)>>16);
    }
  }
  return true;
}
#endif
