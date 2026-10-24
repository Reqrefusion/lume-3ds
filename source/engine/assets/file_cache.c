#include "engine/assets/file_cache.h"
#include <stdlib.h>
#include <string.h>
#include "engine/platform/file_prefetch.h"
bool lume_cache_init(LumeFileCache *c, FILE *f, uint32_t total) {
  memset(c, 0, sizeof(*c));
  c->file = f;
  c->total = total;
  c->capacity = total <= 2 * 1024 * 1024 ? total : 1024 * 1024;
  c->data = malloc(c->capacity);
  return c->data != NULL;
}
const uint8_t *lume_cache_get(LumeFileCache *c, uint32_t offset,
                              uint32_t bytes) {
  const uint8_t *available=NULL;
  int pending=platform_prefetch_poll(c,offset,bytes,&available);
  if(pending)return pending>0?available:NULL;
  if (offset > c->total || bytes > c->total - offset || bytes > c->capacity)
    return NULL;
  if (offset < c->start || offset - c->start > c->size ||
      bytes > c->size - (offset - c->start)) {
    uint32_t start = offset > c->capacity / 4 ? offset - c->capacity / 4 : 0;
    if (bytes > c->capacity - (offset - start))
      start = offset;
    uint32_t n = c->total - start;
    if (n > c->capacity)
      n = c->capacity;
    if (fseek(c->file, start, SEEK_SET) || fread(c->data, 1, n, c->file) != n) {
      c->size = 0;
      return NULL;
    }
    c->start = start;
    c->size = n;
    c->reads++;
  }
  return c->data + offset - c->start;
}
void lume_cache_close(LumeFileCache *c) {
  platform_prefetch_stop(c);
  free(c->data);
  memset(c, 0, sizeof(*c));
}
bool lume_cache_prime(LumeFileCache *c, uint32_t offset, uint32_t bytes) {
  if (bytes > 12 * 1024 * 1024 || offset > c->total ||
      bytes > c->total - offset)
    return false;
  /* The worker owns the FILE cursor and writes c->data. Join before realloc
     or a synchronous seek/read can invalidate memory under the worker. */
  platform_prefetch_stop(c);
  if (bytes > c->capacity) {
    uint8_t *p = realloc(c->data, bytes);
    if (!p)
      return false;
    c->data = p;
    c->capacity = bytes;
    c->size = 0;
  }
  return lume_cache_get(c, offset, bytes) != NULL;
}

bool lume_cache_pending(LumeFileCache *c){return platform_prefetch_pending(c);}
bool lume_cache_prefetch(LumeFileCache *c,uint32_t offset,uint32_t bytes){return platform_prefetch_start(c,offset,bytes);}
