#ifndef LUME_MEDIA_TOP
#define LUME_MEDIA_TOP 13
#define LUME_MEDIA_BOTTOM 226
#endif
#include "profile.h"
#include "audio.h"
#include "engine/core/jobs_state.h"
#include <stdlib.h>
static int tick_compare(const void *a,const void *b) {
  u64 x=*(const u64*)a,y=*(const u64*)b;return (x>y)-(x<y);
}
void lume_profile_frame(u64 start, u64 audio, u64 draw, u64 present, u64 end,
                        int scene, int clip,bool shown) {
  static u64 sums[4], worst,samples[120],window_start;
  static unsigned n,images;
  static int old_scene=-1,old_clip=-1;
  /* Keep scene/clip intervals separate, and count submitted images rather
     than mistaking an idle 60 Hz input loop for 60 unique animation frames. */
  if(scene!=old_scene || clip!=old_clip) {
    for(int i=0;i<4;i++)sums[i]=0;
    worst=0;n=images=0;window_start=start;old_scene=scene;old_clip=clip;
  }
  u64 elapsed = end - start;
  sums[0] += audio - start;
  sums[1] += draw - audio;
  sums[2] += present - draw;
  sums[3] += end - present;
  samples[n++]=elapsed;images+=shown;
  if (elapsed > worst)
    worst = elapsed;
  if (n==120 || end-window_start>=2*SYSCLOCK_ARM11) {
    unsigned submitted,busy;lume_worker_counts(&submitted,&busy);
    lume_log("WORKERS submitted=%u inline_busy=%u",submitted,busy);
    qsort(samples,n,sizeof samples[0],tick_compare);
    double ms=CPU_TICKS_PER_MSEC,wall=(end-window_start)/ms;
    lume_log("PERF scene=%d clip=%d loops=%u images=%u loop_fps=%.2f image_fps=%.2f "
      "audio=%.3f draw=%.3f present=%.3f wait=%.3f p95=%.3f p99=%.3f worst=%.3f",
      scene,clip,n,images,wall>0?1000.*n/wall:0,wall>0?1000.*images/wall:0,
      sums[0]/(n*ms),sums[1]/(n*ms),sums[2]/(n*ms),sums[3]/(n*ms),
      samples[(95*n+99)/100-1]/ms,samples[(99*n+99)/100-1]/ms,worst/ms);
    for (int i = 0; i < 4; i++)
      sums[i] = 0;
    worst = 0;
    n = images = 0;window_start=end;
  }
}
