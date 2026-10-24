/* Internal file-I/O backend; the shared asset cache owns its buffers. */
#ifdef __3DS__
#include "engine/platform/3ds/math.h"
#include <3ds.h>
typedef struct {
  LumeFileCache *cache;
  Thread thread;
  LightLock lock;
  uint32_t offset, bytes;
  bool busy, cancel;
} Prefetch;
static void prefetch_worker(void *arg) {
  Prefetch *p = arg;
  if (!vfp_set_fz_dn()) {
    LightLock_Lock(&p->lock);
    p->busy = false;
    LightLock_Unlock(&p->lock);
    return;
  }
  LumeFileCache *c = p->cache;
  uint32_t done = 0;
  bool ok = fseek(c->file, p->offset, SEEK_SET) == 0;
  while (ok && done < p->bytes) {
    LightLock_Lock(&p->lock);
    bool stop = p->cancel;
    LightLock_Unlock(&p->lock);
    if (stop)
      break;
    uint32_t n = p->bytes - done;
    if (n > 65536)
      n = 65536;
    if (fread(c->data + done, 1, n, c->file) != n)
      break;
    done += n;
    LightLock_Lock(&p->lock);
    c->size = done;
    c->reads++;
    LightLock_Unlock(&p->lock);
  }
  LightLock_Lock(&p->lock);
  p->busy = false;
  LightLock_Unlock(&p->lock);
}
static void platform_prefetch_stop(LumeFileCache *c) {
  Prefetch *p = c->prefetch;
  if (!p)
    return;
  LightLock_Lock(&p->lock);
  p->cancel = true;
  LightLock_Unlock(&p->lock);
  threadJoin(p->thread, U64_MAX);
  threadFree(p->thread);
  free(p);
  c->prefetch = NULL;
}
#else
static void platform_prefetch_stop(LumeFileCache *c) { (void)c; }
#endif
static int platform_prefetch_poll(LumeFileCache *c,uint32_t offset,uint32_t bytes,const uint8_t **out) {
#ifdef __3DS__
  Prefetch *p = c->prefetch;
  if (p) {
    LightLock_Lock(&p->lock);
    bool inside = offset >= p->offset && offset - p->offset <= p->bytes &&
                  bytes <= p->bytes - (offset - p->offset);
    bool available = inside && offset - p->offset <= c->size &&
                     bytes <= c->size - (offset - p->offset);
    bool busy = p->busy;
    LightLock_Unlock(&p->lock);
    if(available){*out=c->data+(offset-p->offset);return 1;}
    if (inside && busy)
      return -1;
    platform_prefetch_stop(c);
  }
#endif
#ifndef __3DS__
 (void)c;(void)offset;(void)bytes;(void)out;
#endif
 return 0;
}
static bool platform_prefetch_pending(LumeFileCache *c) {
#ifdef __3DS__
  Prefetch *p = c->prefetch;
  if (!p)
    return false;
  LightLock_Lock(&p->lock);
  bool busy = p->busy;
  LightLock_Unlock(&p->lock);
  return busy;
#else
  (void)c;
  return false;
#endif
}
static bool platform_prefetch_start(LumeFileCache *c, uint32_t offset, uint32_t bytes) {
#ifdef __3DS__
  if (!bytes || bytes > 12 * 1024 * 1024 || offset > c->total ||
      bytes > c->total - offset)
    return false;
  platform_prefetch_stop(c);
  if(offset>=c->start && offset-c->start<=c->size &&
     bytes<=c->size-(offset-c->start))return true;
  if (bytes > c->capacity) {
    uint8_t *data = realloc(c->data, bytes);
    if (!data)
      return false;
    c->data = data;
    c->capacity = bytes;
  }
  Prefetch *p = calloc(1, sizeof(*p));
  if (!p)
    return false;
  p->cache = c;
  p->offset = offset;
  p->bytes = bytes;
  p->busy = true;
  LightLock_Init(&p->lock);
  c->start = offset;
  c->size = 0;
  c->prefetch = p;
  /* Core 1 is a restricted, single application-thread system slot and can
     already belong to music. I/O can yield on core 0 without stealing it. */
  p->thread = threadCreate(prefetch_worker, p, 16384, 0x32, 0, false);
  if (!p->thread) {
    c->prefetch = NULL;
    free(p);
    c->capacity = c->capacity > 1048576 ? 1048576 : c->capacity;
    return false;
  }
  return true;
#else
  return lume_cache_prime(c, offset, bytes);
#endif
}
