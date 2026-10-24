#ifndef LUME_BLIT_H
#define LUME_BLIT_H
#include <stdint.h>
#include <stdbool.h>
#include "engine/core/jobs.h"
#include "engine/render/colour.h"
void lume_blit_rgba(uint8_t *rgb, const uint8_t *rgba, int w, int h, int x,
                    int y);
/* Clip the destination without editing decoder-owned RGBA reference pixels. */
void lume_blit_rgba_eye(uint8_t *rgb,const uint8_t *rgba,int w,int h,int x,int y,int eye);
/* A signed quarter-pixel plane at full strength. Left/mono stays unchanged;
   the renderer applies the current slider strength exactly once. */
void lume_blit_rgba_parallax(uint8_t *rgb,const uint8_t *rgba,int w,int h,int x,int y,int eye,int quarter_pixels);
void lume_blit_rgba_clipped(uint8_t *rgb,const uint8_t *rgba,int w,int h,int x,int y,
                            int left,int top,int right,int bottom);
void lume_present_bottom(const uint8_t *rgb,uint8_t *fb);
void lume_present_rgb(const uint8_t *left, const uint8_t *right,
                      uint8_t *bottom, uint8_t *top_left, uint8_t *top_right);
typedef struct {
  struct {uint8_t *buffer;unsigned generation,eye;bool valid;} tags[4];
  unsigned next,filtered;
} LumePresentCache;
enum {LUME_TOP_LEFT=1,LUME_TOP_RIGHT=2,LUME_BOTTOM=4};
unsigned lume_present_views_cached(LumePresentCache *cache,unsigned generation,
                      const uint8_t *left,const uint8_t *right,const uint8_t *bottom_rgb,
                      uint8_t *bottom,uint8_t *top_left,uint8_t *top_right);
unsigned lume_present_views_parallel_cached(LumePresentCache *cache,unsigned generation,
                      const uint8_t *left,const uint8_t *right,const uint8_t *bottom_rgb,
                      uint8_t *bottom,uint8_t *top_left,uint8_t *top_right,LumeMovieJobs *jobs);
void lume_present_filter_stats(unsigned *full,unsigned *partial,unsigned *reused,unsigned *saved_rows);
void lume_present_views(const uint8_t *left,const uint8_t *right,const uint8_t *bottom_rgb,
                        uint8_t *bottom,uint8_t *top_left,uint8_t *top_right);
void lume_blit_affine(uint8_t *rgb, const uint8_t *rgba, int w, int h,
                      const float m[6]);

#endif
