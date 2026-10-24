#ifndef SAMOROST_TEXTURE_FILTER_H
#define SAMOROST_TEXTURE_FILTER_H
#include <stdint.h>
/* GPU_RGBA8: alpha, blue, green, red in 8x8 Morton tiles. */
static inline unsigned sm_texel(unsigned x,unsigned y,unsigned width){
 unsigned morton=(x&1)|((y&1)<<1)|((x&2)<<1)|((y&2)<<2)|((x&4)<<2)|((y&4)<<3);
 return (((y>>3)*(width>>3)+(x>>3))*64+morton)*4;
}
static inline unsigned sm_mip_levels(unsigned w,unsigned h){
 unsigned levels=0;while(levels<2&&w>=16&&h>=16){w>>=1;h>>=1;levels++;}return levels;
}
static inline unsigned sm_mip_size(unsigned base,unsigned levels){
 unsigned total=base;while(levels--){base>>=2;total+=base;}return total;
}
static inline void sm_mip_downsample(const uint8_t *src,uint8_t *dst,unsigned width,unsigned height){
 for(unsigned y=0;y<height/2;y++)for(unsigned x=0;x<width/2;x++){
  const uint8_t *p[4]={src+sm_texel(x*2,y*2,width),src+sm_texel(x*2+1,y*2,width),src+sm_texel(x*2,y*2+1,width),src+sm_texel(x*2+1,y*2+1,width)};
  uint8_t *out=dst+sm_texel(x,y,width/2);unsigned alpha=p[0][0]+p[1][0]+p[2][0]+p[3][0];
  out[0]=(uint8_t)((alpha+2)/4);
  /* Average premultiplied colors, then restore straight alpha. Transparent
     atlas padding must not darken grass, ropes or other fine silhouettes. */
  for(unsigned c=1;c<4;c++){unsigned sum=0;for(unsigned i=0;i<4;i++)sum+=p[i][c]*p[i][0];out[c]=alpha?(uint8_t)((sum+alpha/2)/alpha):0;}
 }
}
static inline void sm_mip_generate(uint8_t *data,unsigned w,unsigned h,unsigned levels){
 unsigned bytes=w*h*4;while(levels--){sm_mip_downsample(data,data+bytes,w,h);data+=bytes;w>>=1;h>>=1;bytes>>=2;}
}
#endif
