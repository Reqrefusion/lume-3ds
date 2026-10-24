#ifndef LUME_HOVER_HINTS_H
#define LUME_HOVER_HINTS_H
#include "../../engine/render/colour.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
/* Authored over-state artwork, cropped at original resolution. Decode once;
   never search hit regions or decompress artwork during a frame. */
typedef struct { uint32_t scene,x,y,w,h;uint8_t *rgba; } LumeHover;
static LumeHover lume_hover[2];
static bool lume_hover_ready;
static void lume_hover_load(void) {
 static bool attempted;
 if(attempted)return;
 attempted=true;
 FILE *f=fopen("romfs:/hover.bin","rb");if(!f)return;
 uint32_t h[2];bool ok=fread(h,4,2,f)==2&&!memcmp(h,"LHV1",4)&&h[1]==2;
 for(unsigned i=0;i<2&&ok;i++) {
  LumeHover *p=&lume_hover[i];uint32_t b[5];
  ok=fread(b,4,5,f)==5;if(!ok)break;
  p->scene=b[0];p->x=b[1];p->y=b[2];p->w=b[3];p->h=b[4];
  ok=p->scene==3+i&&p->w&&p->h&&p->w<=800&&p->h<=600&&p->x<=800-p->w&&p->y<=600-p->h;
  if(!ok)break;
  size_t n=p->w*p->h*4;p->rgba=malloc(n);ok=p->rgba&&fread(p->rgba,1,n,f)==n;
 }
 if(ok)ok=fgetc(f)==EOF;
 fclose(f);
 if(!ok)for(unsigned i=0;i<2;i++){free(lume_hover[i].rgba);lume_hover[i].rgba=NULL;}
 lume_hover_ready=ok;
}
/* RGB bottom buffers use the movable 320px camera. Rotated BGR top buffers
   use a fixed camera; both eyes receive an identical flat interface marker.
   Sampling the native crop also keeps the arrow sharp in detail view. */
static void lume_hover_draw(int scene,uint8_t *dst,int width,bool rotated,int cx,int cy,int scale) {
 if(!lume_hover_ready||scene<3||scene>4||!dst||scale<1024||width<1||width>400)return;
 const LumeHover *p=&lume_hover[scene-3];
 int x0=width,y0=240,x1=-1,y1=-1;
 int source_x[400],source_y[240];
 /* The projection is separable. Preserve its exact integer rounding, but
    perform each 64-bit division once per column/row, never per pixel. */
 /* Restrict blending to the projected crop; scan coordinates, not the scene. */
 for(int x=0;x<width;x++) {
  int64_t sx=((int64_t)cx*64+(int64_t)(2*x-width+1)*32768*320*1024/(width*scale))*800/320;
  if(sx>=(int64_t)p->x*65536&&sx<(int64_t)(p->x+p->w)*65536){source_x[x]=(int)(sx/65536)-p->x;if(x<x0)x0=x;x1=x;}
 }
 for(int y=0;y<240;y++) {
  int64_t sy=((int64_t)cy*64+(int64_t)(2*y-239)*32768*1024/scale)*600/240;
  if(sy>=(int64_t)p->y*65536&&sy<(int64_t)(p->y+p->h)*65536){source_y[y]=(int)(sy/65536)-p->y;if(y<y0)y0=y;y1=y;}
 }
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++) {
  int sx=source_x[x],sy=source_y[y];
  const uint8_t *q=p->rgba+((size_t)sy*p->w+sx)*4;unsigned a=q[3];if(!a)continue;
  uint8_t *d=dst+(rotated?(x*240+239-y)*3:(y*width+x)*3);
  const uint8_t ink[3]={q[rotated?2:0],q[1],q[rotated?0:2]};lume_blend_pixel(d,ink,a);
 }
}
#endif
