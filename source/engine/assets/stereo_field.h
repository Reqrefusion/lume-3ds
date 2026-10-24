#ifndef LUME_STEREO_FIELD_H
#define LUME_STEREO_FIELD_H
#include "engine/render/stereo_video.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <lz4.h>
/* Small, offline right-to-left correspondence fields for existing still eye
   pairs. Keep the accepted endpoint image; intermediate views warp geometry
   from the source instead of cross-fading unrelated pixels. */
typedef struct {int8_t *map;uint8_t *image;unsigned w,h,cached;bool valid;} LumeStereoField;
static inline void lume_field_close(LumeStereoField *f) {
 free(f->map);free(f->image);memset(f,0,sizeof *f);
}
static inline bool lume_field_map(LumeStereoField *f,const int8_t *map,unsigned w,unsigned h) {
 lume_field_close(f);if(!w||!h||w>320||h>240)return false;
 /* Keep the requested extent even without a map: animated fallback must
    copy the current frame rather than leaving the other eye stale. */
 f->w=w;f->h=h;
 f->map=malloc(w*h);if(!f->map)return false;
 memcpy(f->map,map,w*h);f->w=w;f->h=h;f->cached=~0u;f->valid=true;return true;
}
static inline bool lume_field_load(LumeStereoField *f,const char *base,unsigned w,unsigned h) {
 lume_field_close(f);if(!w||!h||w>320||h>240)return false;
 /* Keep the requested extent even without a map: animated fallback must
    copy the current frame rather than leaving the other eye stale. */
 f->w=w;f->h=h;
 char path[192];if(snprintf(path,sizeof path,"%s.sdf",base)>=(int)sizeof path)return false;
 FILE *in=fopen(path,"rb");if(!in)return false;
 uint8_t hdr[8];bool ok=!fseek(in,0,SEEK_END);long end=ok?ftell(in):-1;
 ok=ok&&end>8&&end<160000&&!fseek(in,0,SEEK_SET)&&fread(hdr,1,8,in)==8&&
  !memcmp(hdr,"SDF1",4)&&(hdr[4]|(unsigned)hdr[5]<<8)==w&&(hdr[6]|(unsigned)hdr[7]<<8)==h;
 uint8_t *packet=ok?malloc((size_t)end-8):NULL;f->map=ok?malloc(w*h):NULL;
 ok=ok&&packet&&f->map&&fread(packet,1,(size_t)end-8,in)==(size_t)end-8&&
  LZ4_decompress_safe((const char*)packet,(char*)f->map,(int)end-8,(int)(w*h))==(int)(w*h);
 free(packet);fclose(in);
 if(!ok){lume_field_close(f);f->w=w;f->h=h;return false;}
 f->w=w;f->h=h;f->cached=~0u;f->valid=true;return true;
}
/* Animated input must be warped afresh, including at full slider strength.
   The still-image cache and its static right endpoint cannot represent it. */
static inline void lume_field_dynamic(const LumeStereoField *f,const uint8_t *left,
 uint8_t *out,unsigned strength) {
 unsigned w=f->w,h=f->h;
 if(!strength||!f->valid){if(out!=left)memcpy(out,left,w*h*3);return;}
 if(strength>256)strength=256;
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++) {
  int q=(int)(x*1024)+f->map[y*w+x]*(int)strength;
  if(q<0)q=0;
  if(q>(int)(w-1)*1024)q=(int)(w-1)*1024;
  unsigned a=(unsigned)q>>10,b=a+1<w?a+1:a,t=(unsigned)q&1023;
  const uint8_t *p=left+(y*w+a)*3;uint8_t *dst=out+(y*w+x)*3;
  if(!t){dst[0]=p[0];dst[1]=p[1];dst[2]=p[2];continue;}
  const uint8_t *next=left+(y*w+b)*3;
  for(unsigned c=0;c<3;c++)dst[c]=(uint8_t)((p[c]*(1024-t)+next[c]*t+512)>>10);
 }
}
static inline void lume_field_draw(const LumeStereoField *f,const uint8_t *left,const uint8_t *full,
 uint8_t *out,unsigned strength) {
 if(strength>=256||!f->valid){
  const uint8_t *src=strength?full:left;
  if(out!=src)memcpy(out,src,f->w*f->h*3);
  return;
 }
 lume_field_dynamic(f,left,out,strength);
}
static inline const uint8_t *lume_field_view(LumeStereoField *f,const uint8_t *left,const uint8_t *full) {
 unsigned strength=lume_stereo_strength();
 if(!strength)return left;
 if(strength==256||!f->valid)return full;
 if(!f->image)f->image=malloc(f->w*f->h*3);
 if(!f->image)return full;
 if(f->cached!=strength){lume_field_draw(f,left,full,f->image,strength);f->cached=strength;}
 return f->image;
}
#endif
