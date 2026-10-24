#ifndef LUME_PIXEL_PREDICT_H
#define LUME_PIXEL_PREDICT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Reconstruct exact source bytes in the existing output buffer. Predictors
   depend only on this crop, so jumping to another frame needs no preroll. */
static inline bool lume_pixel_restore(uint8_t *p,unsigned w,unsigned h,
                                      unsigned channels,unsigned mode) {
 if(mode>2 || !w || !h || !channels)return mode==0;
 size_t stride=(size_t)w*channels,bytes=stride*h;
 if(mode==1) {
  for(size_t row=0;row<bytes;row+=stride)
   for(size_t i=row+channels;i<row+stride;i++)p[i]=(uint8_t)(p[i]+p[i-channels]);
 } else if(mode==2) {
  for(size_t i=stride;i<bytes;i++)p[i]=(uint8_t)(p[i]+p[i-stride]);
 }
 return true;
}
#endif
