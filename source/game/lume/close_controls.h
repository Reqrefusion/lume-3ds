#include "storage.h"
#ifndef LUME_CLOSE_CONTROLS_H
#define LUME_CLOSE_CONTROLS_H
#include "game.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
enum { LUME_CLOSE_NONE,LUME_CLOSE_HELP,LUME_CLOSE_PUZZLE,LUME_CLOSE_CLUE,LUME_CLOSE_PANTRY };
static inline int lume_close_target(const LumeGame *g,bool debug,bool help){
 if(help)return LUME_CLOSE_HELP;
 if(debug||g->puzzle>=0)return LUME_CLOSE_PUZZLE;
 if(g->clue>=0)return LUME_CLOSE_CLUE;
 return g->scene==PANTRY?LUME_CLOSE_PANTRY:LUME_CLOSE_NONE;
}
/* The close control is UI, never baked into scene or stereo images. */
static uint8_t lume_close_rgba[32*32*4];
static unsigned lume_close_x,lume_close_y,lume_close_w,lume_close_h;
static bool lume_close_ready;
static inline bool lume_close_load(void){
 lume_close_ready=false;
 FILE *f=fopen(LUME_DATA_FILE("ui_close.bin"),"rb");if(!f)return false;
 uint8_t h[12]={0};bool ok=fread(h,1,12,f)==12&&!memcmp(h,"XUI1",4);
 unsigned x=h[4]|(unsigned)h[5]<<8,y=h[6]|(unsigned)h[7]<<8,w=h[8]|(unsigned)h[9]<<8,ht=h[10]|(unsigned)h[11]<<8;
 ok=ok&&w&&w<=32&&ht&&ht<=32&&x+w<=29&&y>=13&&y+ht<=44;
 if(ok)ok=fread(lume_close_rgba,4,w*ht,f)==w*ht&&fgetc(f)==EOF&&!ferror(f);
 fclose(f);lume_close_ready=ok;
 if(ok){lume_close_x=x;lume_close_y=y;lume_close_w=w;lume_close_h=ht;}return ok;
}
/* Draw after the fresh bottom view is presented: no extra full-screen copy. */
static inline void lume_close_draw_bottom(uint8_t *fb){
 if(!lume_close_ready)return;
 const uint8_t *p=lume_close_rgba;
 for(unsigned y=0;y<lume_close_h;y++)for(unsigned x=0;x<lume_close_w;x++,p+=4){
  uint8_t *d=fb+((x+lume_close_x)*240+239-y-lume_close_y)*3;
  for(unsigned c=0;c<3;c++)d[c]=(p[2-c]*p[3]+d[c]*(255-p[3])+127)/255;
 }
}
#endif
