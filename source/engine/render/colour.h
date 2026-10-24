#ifndef LUME_COLOUR_H
#define LUME_COLOUR_H
#include <stdint.h>
/* Exact rounded /255 blend in two independent 16-bit colour lanes. */
static inline void lume_blend_pixel(uint8_t *dst,const uint8_t *src,unsigned alpha) {
  unsigned inv=255-alpha;
  uint32_t rb=(src[0]|(uint32_t)src[2]<<16)*alpha+
              (dst[0]|(uint32_t)dst[2]<<16)*inv+0x7f007fu;
  rb=(rb+0x10001u+((rb>>8)&0xff00ffu))>>8;
  unsigned g=src[1]*alpha+dst[1]*inv+127;
  dst[0]=rb;dst[1]=(g+1+(g>>8))>>8;dst[2]=rb>>16;
}
/* RGB already contains its alpha: scale the destination only. Keeping
   red/blue in independent lanes preserves the old per-channel rounding. */
static inline void lume_blend_premultiplied(uint8_t *dst,const uint8_t *src,unsigned alpha) {
 unsigned inv=255-alpha;
 uint32_t rb=(dst[0]|(uint32_t)dst[2]<<16)*inv+0x7f007fu;
 rb=(rb+0x10001u+((rb>>8)&0xff00ffu))>>8;
 unsigned g=dst[1]*inv+127;
 dst[0]=src[0]+(rb&255);dst[1]=src[1]+((g+1+(g>>8))>>8);
 dst[2]=src[2]+((rb>>16)&255);
}
#endif
