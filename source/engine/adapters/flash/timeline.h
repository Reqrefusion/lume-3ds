#ifndef LUME_ANIMATION_H
#define LUME_ANIMATION_H
#include "engine/assets/file_cache.h"
#include "engine/codecs/lan.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <zlib.h>
typedef struct {
  FILE *file;
  LumeFileCache cache;
  uint32_t *offsets;
  unsigned fps, frames, movie, background;
  uint16_t *cues;
  uint32_t *cue_offsets;
  uint16_t *cue_ids;
  unsigned cue_count;
  uint8_t *packet, *rgba;
  size_t packet_capacity;
  unsigned prepared, width, height;
  int x, y;
  bool mixed_codec, spatial_codec, temporal_codec;
  LumeLanState lan_state;
  uint32_t file_size;
  unsigned reads, decodes, fast_decodes;
} LumeAnimation;
bool lume_animation_open(LumeAnimation *a, const char *path);
bool lume_animation_frame(LumeAnimation *a, unsigned frame, uint8_t *rgb,
                          int eye);
void lume_animation_close(LumeAnimation *a);
bool lume_animation_camera_at(LumeAnimation *a,unsigned frame,unsigned *movie,unsigned *background);
bool lume_animation_camera_range(LumeAnimation *a, unsigned *movie,
                                 unsigned *first, unsigned *last);
#include "engine/core/timeline_clock.h"
#endif
