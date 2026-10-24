#ifndef LUME_FILE_CACHE_H
#define LUME_FILE_CACHE_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
typedef struct {
  FILE *file;
  uint8_t *data;
  uint32_t total, start, size, capacity;
  unsigned reads;
  void *prefetch;
} LumeFileCache;
bool lume_cache_init(LumeFileCache *c, FILE *f, uint32_t total);
const uint8_t *lume_cache_get(LumeFileCache *c, uint32_t offset,
                              uint32_t bytes);
void lume_cache_close(LumeFileCache *c);
bool lume_cache_prime(LumeFileCache *c, uint32_t offset, uint32_t bytes);

bool lume_cache_prefetch(LumeFileCache*,uint32_t,uint32_t);
bool lume_cache_pending(LumeFileCache*);
#endif
