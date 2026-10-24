#ifndef LUME_VIDEO_TEXTURE_H
#define LUME_VIDEO_TEXTURE_H
#include <stdint.h>
#include <stddef.h>
/* RGBA8 PICA texture words are A,B,G,R in memory. Source remains RGB888.
   One extra edge row/column repeats the final texel for linear filtering. */
static inline unsigned lume_video_morton(unsigned x,unsigned y) {
 return (x&1u)|((y&1u)<<1)|((x&2u)<<1)|((y&2u)<<2)|((x&4u)<<2)|((y&4u)<<3);
}
static inline size_t lume_video_texel(unsigned x,unsigned y) {
 return ((size_t)(y/8)*64+x/8)*64+lume_video_morton(x&7,y&7);
}
static inline uint32_t lume_video_rgba(const uint8_t *p) {
 return 255u|(uint32_t)p[2]<<8|(uint32_t)p[1]<<16|(uint32_t)p[0]<<24;
}
static inline void lume_video_texture_pack(uint32_t *out,const uint8_t *rgb,unsigned first,unsigned height) {
 /* Walk 8x8 tiles, writing each 2x2 Morton group as four consecutive words.
    This keeps writes in one cache line instead of revisiting 40 tiles per row. */
 unsigned full=height&~7u;
 static const unsigned columns[4]={0,4,16,20};
 static const unsigned rows[4]={0,8,32,40};
 for(unsigned ty=0;ty<full;ty+=8)for(unsigned tx=0;tx<320;tx+=8) {
  uint32_t *tile=out+(size_t)(ty/8)*512*8+(tx/8)*64;
  const uint8_t *source=rgb+((size_t)(first+ty)*320+tx)*3;
  for(unsigned y=0;y<4;y++) {
   const uint8_t *p=source+y*2*320*3;
   for(unsigned x=0;x<4;x++,p+=6) {
    uint32_t *d=tile+rows[y]+columns[x];
    d[0]=lume_video_rgba(p);d[1]=lume_video_rgba(p+3);
    d[2]=lume_video_rgba(p+320*3);d[3]=lume_video_rgba(p+320*3+3);
   }
  }
 }
 /* Partial final tile row, and the filtering guard below the crop. */
 for(unsigned y=full;y<=height;y++) {
  unsigned sy=first+(y<height?y:height-1);
  for(unsigned x=0;x<320;x++)out[lume_video_texel(x,y)]=lume_video_rgba(rgb+((size_t)sy*320+x)*3);
 }
 /* Filtering guard at the right edge; unused texture bytes stay untouched. */
 for(unsigned y=0;y<=height;y++) {
  unsigned sy=first+(y<height?y:height-1);
  out[lume_video_texel(320,y)]=lume_video_rgba(rgb+((size_t)sy*320+319)*3);
 }
}
#endif
