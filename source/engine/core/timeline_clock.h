#ifndef LUME_ENGINE_TIMELINE_CLOCK_H
#define LUME_ENGINE_TIMELINE_CLOCK_H
#include <stdint.h>
/* Return frames as the finish sentinel only after the terminal frame was
 * composed once. Slow drawing must not silently drop the last art/cues. */
static inline unsigned lume_animation_clock_frame(uint64_t now,uint64_t started,
    unsigned fps,unsigned frames,unsigned composed) {
  if(!fps || !frames)return frames;
  uint64_t elapsed=now>=started?now-started:0;
  if(elapsed>=((uint64_t)frames*1000+fps-1)/fps)
    return composed==frames-1?frames:frames-1;
  return (unsigned)(elapsed*fps/1000);
}

/* A producer/IO stall retries the requested authored pose. Do not add the
   whole previous draw interval after motion_frame already corrected a late
   clock: that would push the clock into the future and double the slowdown. */
static inline uint64_t lume_animation_retry_start(uint64_t now,unsigned fps,unsigned frame) {
 uint64_t elapsed=fps?((uint64_t)frame*1000+fps-1)/fps:0;
 return now>=elapsed?now-elapsed:0;
}

/* AVM1 advances one authored frame per tick. Preserve short navigation and
   character poses on slow devices, including their first and terminal frames.
   A delayed draw stretches this clip's clock; it never skips to a later pose. */
static inline unsigned lume_animation_motion_frame(uint64_t now,uint64_t *started,
    unsigned fps,unsigned frames,unsigned composed) {
 if(!fps||!frames)return frames;
 if(composed==~0u){*started=now;return 0;}
 unsigned due=lume_animation_clock_frame(now,*started,fps,frames,composed);
 if(composed<frames-1 && due>composed+1) {
  due=composed+1;
  uint64_t elapsed=((uint64_t)due*1000+fps-1)/fps;
  *started=now>=elapsed?now-elapsed:0;
 }
 return due;
}


#endif
