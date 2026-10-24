#ifndef LUME_GPU_VIDEO_PRESENT_H
#define LUME_GPU_VIDEO_PRESENT_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
 unsigned frames,failures,reused,transfers;bool attempted,ready;
 const char *state;
 uint64_t upload_ticks,render_ticks,copy_ticks;
} LumeGpuVideoStats;
/* NULL right input/output selects the left eye. Required left pointers must
   be valid. False means the caller must overwrite outputs with its CPU path.
   After close a new present may initialize again; failures stay latched until
   explicit close, so an unavailable backend is not retried every frame. */
bool lume_gpu_video_present(unsigned serial,const uint8_t *left,const uint8_t *right,uint8_t *fb_left,uint8_t *fb_right);
const LumeGpuVideoStats *lume_gpu_video_stats(void);
void lume_gpu_video_close(void);
void lume_gpu_video_invalidate(void);
#endif
