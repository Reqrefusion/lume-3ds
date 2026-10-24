#ifndef LUME_STEREO_VIDEO_H
#define LUME_STEREO_VIDEO_H
#include <stdint.h>
/* LVS1 payload: RGB565 left image, then signed quarter-pixel disparity.
   A NULL right output skips the hidden eye without changing the left image. */
void lume_decode_stereo565(const uint8_t*,uint8_t*,uint8_t*);
/* Main-thread display setting. Workers use a movie-owned snapshot instead. */
unsigned lume_stereo_strength(void);
void lume_stereo_set_strength(unsigned strength);
void lume_decode_stereo565_strength(const uint8_t*,uint8_t*,uint8_t*,unsigned);
#endif
