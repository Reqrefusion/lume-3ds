#ifndef LUME_MOVIE_H
#define LUME_MOVIE_H
#include "engine/assets/file_cache.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
typedef struct {
  uint64_t codec_ticks,convert_ticks,depth_ticks,depth_wait_ticks,stereo_ticks;
  uint32_t decoded_frames,conversions,depth_frames,depth_skipped,depth_jobs,stereo_jobs,stereo_frames,rewinds,cached_frames;
  uint64_t lookahead_wait_ticks;
  uint32_t lookahead_requests,lookahead_hits,lookahead_misses,lookahead_busy_polls;
  uint64_t colour_hw_ticks,colour_sw_ticks;
  uint32_t colour_samples;
  uint32_t y2r_frames,y2r_fallbacks,y2r_mae_milli,y2r_direct_frames;
  int worker_core;
  const char *y2r_failure;int32_t y2r_result;
} LumeMovieStats;
typedef struct {
  LumeMovieStats stats;
  FILE *file;
  LumeFileCache cache;
  unsigned fps, frames, last,stereo_strength;
  uint32_t *offsets;
  bool blocked, disparity, last_stereo, temporal, mpeg;
  unsigned mpeg_group_limit;
  void *mpeg_state,*lookahead;
  bool lookahead_tried;
  unsigned raw_last;
  uint8_t *residual;
  uint8_t *blocks;
} LumeMovie;
void lume_movie_set_strength(LumeMovie *m,unsigned strength);
bool lume_movie_copy_disparity(LumeMovie *m,int8_t *out);
/* Borrow only an already decoded, exact-frame field. No wait, allocation,
   I/O or depth reconstruction. Consume before scheduling another frame. */
const int8_t *lume_movie_disparity_frame(const LumeMovie *m,unsigned frame);
bool lume_movie_open(LumeMovie *m, const char *path);
/* A NULL right output requests only the visible left eye. Re-enabling the
   right eye on the same frame decodes it before marking the cache complete. */
bool lume_movie_frame(LumeMovie *m, unsigned frame, uint8_t *left,
                      uint8_t *right);
/* Only the movie owns its decoder/FILE. A bounded, single future frame can
   overlap the caller's composition/presentation on permitted New hardware. */
bool lume_movie_hint(LumeMovie *m,unsigned frame,bool stereo);
bool lume_movie_ready(LumeMovie *m);
void lume_movie_sync(LumeMovie *m);
void lume_movie_close(LumeMovie *m);
bool lume_movie_prefetch(LumeMovie *m, unsigned first, unsigned last);
bool lume_movie_prime(LumeMovie *m, unsigned first, unsigned last);

#endif
