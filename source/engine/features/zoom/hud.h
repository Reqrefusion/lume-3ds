#ifndef LUME_ZOOM_HUD_H
#define LUME_ZOOM_HUD_H
#include "engine/render/colour.h"
#include "engine/features/ui/menu_art.h"
/* A small paper badge uses source glyphs and a supersampled magnifying glass.
   Masks are built once; only the final physical bottom-screen pixels blend. */
enum{LUME_ZOOM_X=222,LUME_ZOOM_Y=6,LUME_ZOOM_W=90,LUME_ZOOM_H=32};
static inline void lume_zoom_hud(uint8_t *rgb,unsigned scale){
 if(scale<=1024)return;
 static bool ready;static uint8_t paper[LUME_ZOOM_W*LUME_ZOOM_H],glass[400];
 if(!ready){
  for(int y=0;y<LUME_ZOOM_H;y++)for(int x=0;x<LUME_ZOOM_W;x++){
   int cx=x<8?8:x>=LUME_ZOOM_W-8?LUME_ZOOM_W-9:x;
   int cy=y<8?8:y>=LUME_ZOOM_H-8?LUME_ZOOM_H-9:y;unsigned samples=0;
   for(int sy=0;sy<4;sy++)for(int sx=0;sx<4;sx++){
    int dx=4*(x-cx)+sx-1,dy=4*(y-cy)+sy-1;samples+=dx*dx+dy*dy<=32*32;
   }
   paper[y*LUME_ZOOM_W+x]=(samples*232+8)/16;
  }
  for(int y=0;y<20;y++)for(int x=0;x<20;x++){
   unsigned samples=0;for(int sy=0;sy<4;sy++)for(int sx=0;sx<4;sx++){
    int xx=x*4+sx,yy=y*4+sy,dx=xx-28,dy=yy-28,rr=dx*dx+dy*dy;
    bool ring=rr>=17*17&&rr<=23*23;
    bool handle=xx>=44&&yy>=44&&xx<=71&&yy<=71&&xx-yy>=-5&&xx-yy<=5;
    samples+=ring||handle;
   }glass[y*20+x]=(samples*255+8)/16;
  }ready=true;
 }
 for(unsigned y=0;y<LUME_ZOOM_H;y++)for(unsigned x=0;x<LUME_ZOOM_W;x++){
  unsigned a=paper[y*LUME_ZOOM_W+x];if(!a)continue;
  bool edge=y==0||y==LUME_ZOOM_H-1||x==0||x==LUME_ZOOM_W-1||a<200;
  uint8_t ink[3]={edge?125:246-y*16/31,edge?105:232-y*22/31,edge?74:204-y*26/31};
  uint8_t *p=rgb+((LUME_ZOOM_Y+y)*320+LUME_ZOOM_X+x)*3;
  lume_blend_pixel(p,ink,a);
 }
 for(unsigned y=0;y<20;y++)for(unsigned x=0;x<20;x++){
  unsigned a=glass[y*20+x];uint8_t *p=rgb+((LUME_ZOOM_Y+6+y)*320+LUME_ZOOM_X+7+x)*3;
  const uint8_t ink[3]={63,80,67};lume_blend_pixel(p,ink,a);
 }
 char label[16];unsigned tenths=(scale*10+512)/1024;snprintf(label,sizeof label,"%u.%ux",tenths/10,tenths%10);
 lume_menu_art_text(NULL,rgb,LUME_ZOOM_X+34,LUME_ZOOM_Y+6,label,0x3d382b,2);
}
#endif
