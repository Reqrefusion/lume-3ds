#ifndef LUME_PUZZLE_DEPTH_H
#define LUME_PUZZLE_DEPTH_H
#include <stdint.h>
#include <string.h>
#define LUME_PUZZLE_DEPTH_BYTES (8 + 320 * 240)
/* Each row is independent. The quarter-pixel interpolation and rounding
   match the source warp; callers can assign disjoint rows to a worker. */
static inline void lume_puzzle_reproject_rows(const uint8_t *left,uint8_t *right,
                                             const int8_t *disparity,unsigned first,unsigned last){
 if(first>last || last>240)return;
 if(!disparity){memcpy(right+first*960,left+first*960,(last-first)*960);return;}
 static const int8_t zero[320];
 uint32_t packed[320]; /* Invocation-owned: two workers may render disjoint rows. */
 for(unsigned y=first;y<last;y++){
  const uint8_t *row=left+y*960;uint8_t *out=right+y*960;
  const int8_t *depth=disparity+y*320;
  if(!memcmp(depth,zero,sizeof zero)){memcpy(out,row,960);continue;}
  for(unsigned x=0;x<320;x++)
   packed[x]=row[x*3]|(uint32_t)row[x*3+1]<<8|(uint32_t)row[x*3+2]<<16;
  for(unsigned x=0;x<320;x++,out+=3){
   int q=(int)x*4+depth[x];
   if(q<0)q=0;
   if(q>1276)q=1276;
   unsigned sx=(unsigned)q>>2,f=(unsigned)q&3;
   uint32_t c=packed[sx];
   if(f){
    uint32_t next=packed[sx<319?sx+1:sx];unsigned inv=4-f;
    uint32_t rb=(((c&0xff00ffu)*inv+(next&0xff00ffu)*f+0x20002u)>>2)&0xff00ffu;
    uint32_t g=(((c&0xff00u)*inv+(next&0xff00u)*f+0x200u)>>2)&0xff00u;
    c=rb|g;
   }
   out[0]=(uint8_t)c;out[1]=(uint8_t)(c>>8);out[2]=(uint8_t)(c>>16);
  }
 }
}
/* Scale geometry, never dissolve the two eye images. The original map is
   immutable; even a held camera can be reprojected at a new slider value. */
static inline void lume_puzzle_reproject_strength_rows(const uint8_t *left,uint8_t *right,
 const int8_t *disparity,unsigned strength,unsigned first,unsigned last) {
 if(strength>=256){lume_puzzle_reproject_rows(left,right,disparity,first,last);return;}
 if(first>last||last>240)return;
 if(!strength||!disparity){memcpy(right+first*960,left+first*960,(last-first)*960);return;}
 for(unsigned y=first;y<last;y++)for(unsigned x=0;x<320;x++) {
  int q=(int)(x*1024)+disparity[y*320+x]*(int)strength;
  if(q<0)q=0;
  if(q>319*1024)q=319*1024;
  unsigned a=(unsigned)q>>10,f=(unsigned)q&1023,b=a<319?a+1:a;
  const uint8_t *p=left+(y*320+a)*3;uint8_t *dst=right+(y*320+x)*3;
  /* Zero depth and clamped edges need no interpolation, at every slider
     value. Preserve the source pixels directly instead of mixing them. */
  if(!f){dst[0]=p[0];dst[1]=p[1];dst[2]=p[2];continue;}
  const uint8_t *next=left+(y*320+b)*3;
  for(unsigned c=0;c<3;c++)dst[c]=(uint8_t)((p[c]*(1024-f)+next[c]*f+512)>>10);
 }
}
static inline void lume_puzzle_reproject_strength(const uint8_t *left,uint8_t *right,
 const int8_t *disparity,unsigned strength) {
 lume_puzzle_reproject_strength_rows(left,right,disparity,strength,0,240);
}
/* Reproject the complete current board, including its changing controls. */
static inline void lume_puzzle_reproject(const uint8_t *left,uint8_t *right,
                                         const int8_t *disparity){
 lume_puzzle_reproject_rows(left,right,disparity,0,240);
}
#endif
