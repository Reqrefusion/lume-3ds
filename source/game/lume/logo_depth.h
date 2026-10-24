#ifndef LUME_LOGO_DEPTH_H
#define LUME_LOGO_DEPTH_H
#include <stdint.h>
#include <string.h>
#define LUME_LOGO_QUARTER_PIXELS 12
#define LUME_LOGO_ENDING_FIRST_FRAME 1883
/* The ending's full black cover stays fixed. Shift only the logo region,
   otherwise moving that cover would reveal the garden along a screen edge. */
static inline void lume_logo_front(const uint8_t *left,uint8_t *right,unsigned strength,
 unsigned x0,unsigned y0,unsigned x1,unsigned y1){
 if(strength>256)strength=256;
 uint8_t row[960];unsigned offset=LUME_LOGO_QUARTER_PIXELS*strength;
 for(unsigned y=y0;y<y1;y++){
  memcpy(row,left+y*960,sizeof row);
  for(unsigned x=x0;x<x1;x++){
   unsigned q=x*1024+offset,a=q/1024,b=a<319?a+1:a,t=q%1024;
   for(unsigned c=0;c<3;c++)right[y*960+x*3+c]=(uint8_t)((row[a*3+c]*(1024-t)+row[b*3+c]*t+512)/1024);
  }
 }
}
static inline void lume_logo_ending_front(uint8_t *rgb,unsigned strength){
 if(strength)lume_logo_front(rgb,rgb,strength,102,18,218,62);
}
static inline void lume_logo_title_front(const uint8_t *left,uint8_t *right,unsigned strength){
 lume_logo_front(left,right,strength,70,34,254,105);
}
#endif
