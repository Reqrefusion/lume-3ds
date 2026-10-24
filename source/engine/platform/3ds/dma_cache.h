#ifndef LUME_DMA_CACHE_H
#define LUME_DMA_CACHE_H
#include <3ds.h>
#include <stdint.h>
/* Use the current process for buffers owned by a decoder worker as well.
   GSP service cache operations can reject those virtual addresses on hardware. */
static inline Result lume_dma_flush(const void *p,u32 bytes) {
 Result rc=svcFlushProcessDataCache(CUR_PROCESS_HANDLE,(u32)(uintptr_t)p,bytes);
 return R_FAILED(rc)?GSPGPU_FlushDataCache(p,bytes):rc;
}
static inline Result lume_dma_invalidate(const void *p,u32 bytes) {
 Result rc=svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE,(u32)(uintptr_t)p,bytes);
 return R_FAILED(rc)?GSPGPU_InvalidateDataCache(p,bytes):rc;
}
#endif
