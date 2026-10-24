#ifndef LUME_FRAME_FLUSH_H
#define LUME_FRAME_FLUSH_H
#include "engine/platform/3ds/dma_cache.h"
#include <stdint.h>
#include "engine/render/blit.h"
static inline void lume_flush_region(const void *data,u32 bytes) {
 (void)lume_dma_flush(data,bytes);
}
static inline void lume_flush_framebuffers(unsigned dirty,const void *bottom,const void *left,const void *right) {
 if(dirty&LUME_TOP_LEFT)lume_flush_region(left,400*240*3);
 if((dirty&LUME_TOP_RIGHT) && (!(dirty&LUME_TOP_LEFT) || right!=left))
  lume_flush_region(right,400*240*3);
 if(dirty&LUME_BOTTOM)lume_flush_region(bottom,320*240*3);
}
#endif
