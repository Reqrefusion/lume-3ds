#ifndef LUME_NATIVE_MENU_ART_H
#define LUME_NATIVE_MENU_ART_H
#include "engine/render/colour.h"
#include "engine/features/ui/backend.h"
#include "engine/features/locale/locale.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define LUME_MENU_ART_COUNT 11
#define LUME_MENU_ART_BYTES 344448
static const unsigned lume_menu_art_width[11]={64,64,64,64,64,64,64,200,216,216,216};
static const unsigned lume_menu_art_height[11]={64,64,64,64,64,64,64,28,80,80,80};
typedef struct {uint8_t *rgba;uint8_t font[95*240];bool glyph[95],attempted,ready;} LumeMenuArt;
static inline void lume_menu_art_close(LumeMenuArt *a){free(a->rgba);memset(a,0,sizeof *a);}
static inline bool lume_menu_art_load(LumeMenuArt *a){
 if(a->attempted)return a->ready;
 a->attempted=true;FILE *f=lume_ui_asset_open("detail/native_menu/art.bin");if(!f)return false;
 uint8_t h[8];bool ok=fread(h,1,8,f)==8&&!memcmp(h,"LNA1\013\000\000\000",8);
 for(unsigned i=0;i<11&&ok;i++){
  uint8_t dims[4];ok=fread(dims,1,4,f)==4&&(unsigned)(dims[0]|dims[1]<<8)==lume_menu_art_width[i]&&(unsigned)(dims[2]|dims[3]<<8)==lume_menu_art_height[i];
 }
 if(ok)a->rgba=malloc(LUME_MENU_ART_BYTES);
 ok=ok&&a->rgba&&fread(a->rgba,1,LUME_MENU_ART_BYTES,f)==LUME_MENU_ART_BYTES&&fgetc(f)==EOF&&!ferror(f);fclose(f);
 if(ok)for(unsigned i=0;i<LUME_MENU_ART_BYTES;i+=4)
  if(a->rgba[i]>a->rgba[i+3]||a->rgba[i+1]>a->rgba[i+3]||a->rgba[i+2]>a->rgba[i+3]){ok=false;break;}
 if(ok){
  f=lume_ui_asset_open("detail/native_menu/font.bin");
  ok=f&&fread(h,1,8,f)==8&&!memcmp(h,"LRF1\014\024\137\000",8)&&fread(a->font,1,sizeof a->font,f)==sizeof a->font&&fgetc(f)==EOF&&!ferror(f);
  if(f)fclose(f);
 }
 if(!ok){free(a->rgba);a->rgba=NULL;return false;}
 for(unsigned c=0;c<95;c++)for(unsigned i=0;i<240;i++)if(a->font[c*240+i]){a->glyph[c]=true;break;}
 a->ready=true;return true;
}
/* Final-size premultiplied source pixels are blended directly. There is no
   40px toolbar enlargement, bitmap font enlargement or per-frame resampling. */
static inline void lume_menu_art_blit(const LumeMenuArt *a,unsigned index,uint8_t *rgb,int dx,int dy){
 if(!a||!a->ready||index>=11)return;
 unsigned offset=0;for(unsigned i=0;i<index;i++)offset+=lume_menu_art_width[i]*lume_menu_art_height[i]*4;
 unsigned w=lume_menu_art_width[index],h=lume_menu_art_height[index];
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
  int tx=dx+(int)x,ty=dy+(int)y;if((unsigned)tx>=320||(unsigned)ty>=240)continue;
  const uint8_t *p=a->rgba+offset+(y*w+x)*4;unsigned alpha=p[3];if(!alpha)continue;
  uint8_t *d=rgb+(ty*320+tx)*3;
  lume_blend_premultiplied(d,p,alpha);
 }
}
static inline void lume_menu_art_text(const LumeMenuArt *a,uint8_t *rgb,int x,int y,const char *text,uint32_t color,unsigned scale){
 if(lume_locale.font_ready){lume_locale_draw(rgb,320,x,y,text,color,scale);return;}
 for(;*text;text++,x+=(int)(6*scale)){
  unsigned c=(unsigned char)*text;if(c<32||c>126)c='?';
  if(c==' ')continue;
  if(!a||!a->ready||!a->glyph[c-32]){char fallback[2]={(char)c,0};lume_ui_fallback_text(rgb,x,y,fallback,color,(int)scale);continue;}
  for(unsigned yy=0;yy<10*scale;yy++)for(unsigned xx=0;xx<6*scale;xx++){
   unsigned alpha;
   if(scale==2)alpha=a->font[(c-32)*240+yy*12+xx];
   else {unsigned i=(c-32)*240+yy*24+xx*2;alpha=(a->font[i]+a->font[i+1]+a->font[i+12]+a->font[i+13]+2)/4;}
   int tx=x+(int)xx,ty=y+(int)yy;if(!alpha||(unsigned)tx>=320||(unsigned)ty>=240)continue;
   uint8_t *p=rgb+(ty*320+tx)*3;uint8_t ink[3]={color>>16&255,color>>8&255,color&255};
   lume_blend_pixel(p,ink,alpha);
  }
 }
}
#endif
