#ifndef LUME_SKIP_BUTTON_H
#define LUME_SKIP_BUTTON_H
#include "engine/render/colour.h"
#include "engine/render/blit.h"
#include "engine/features/ui/menu_art.h"
#include <string.h>
/* UI stays in physical lower-screen coordinates, outside movie zoom/stereo.
   Source btnSkip=913 appears only in intro and ending, not walking clips. */
enum {LUME_SKIP_X=200,LUME_SKIP_Y=192,LUME_SKIP_W=112,LUME_SKIP_H=40};
static inline bool lume_skip_rect_hit(int x,int y){return x>=LUME_SKIP_X&&x<LUME_SKIP_X+LUME_SKIP_W&&y>=LUME_SKIP_Y&&y<LUME_SKIP_Y+LUME_SKIP_H;}
/* This control is text, rather than a baked English sprite. Reuse the
   source font and muted green palette at physical lower-screen resolution. */
static void lume_skip_draw(uint8_t *rgb) {
 const char *text=lume_tr("skip","Skip");unsigned scale=strlen(text)<=7?2:1;
 char label[15];snprintf(label,sizeof label,"%.14s",text);
 for(int y=0;y<LUME_SKIP_H;y++)for(int x=0;x<LUME_SKIP_W;x++){
  int cx=x<8?8:x>=LUME_SKIP_W-8?LUME_SKIP_W-9:x;
  int cy=y<8?8:y>=LUME_SKIP_H-8?LUME_SKIP_H-9:y;
  int distance=(x-cx)*(x-cx)+(y-cy)*(y-cy);
  if(distance>64)continue;
  bool edge=x==0||x==LUME_SKIP_W-1||y==0||y==LUME_SKIP_H-1||distance>49;
  unsigned alpha=edge?170:190;uint8_t ink[3]={edge?96:23,edge?151:37,edge?125:32};
  uint8_t *p=rgb+((LUME_SKIP_Y+y)*320+LUME_SKIP_X+x)*3;
  lume_blend_pixel(p,ink,alpha);
 }
 int width=(int)strlen(label)*6*(int)scale;
 lume_menu_art_text(NULL,rgb,LUME_SKIP_X+(LUME_SKIP_W-width)/2,
                    LUME_SKIP_Y+(LUME_SKIP_H-10*(int)scale)/2,label,0xa5d4b9,scale);
}
/* The source exporter removes btnSkip before generating the overlay.
   Never clear a rectangle here: a cinematic photo can be a foreground
   layer, and clearing it exposes a different plate as a visible square. */
#endif
