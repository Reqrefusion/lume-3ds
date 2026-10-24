#include "engine/codecs/pixel_predict.h"
#include "engine/adapters/flash/timeline.h"
#include "engine/render/blit.h"
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <lz4.h>
static uint32_t le(const uint8_t *p) {
  return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}
void lume_animation_close(LumeAnimation *a) {
  lume_lan_close(&a->lan_state);
  lume_cache_close(&a->cache);
  if (a->file)
    fclose(a->file);
  free(a->offsets);
  free(a->cues);
  free(a->cue_offsets);
  free(a->cue_ids);
  free(a->packet);
  free(a->rgba);
  memset(a, 0, sizeof(*a));
}
/* Build a sidecar name without truncating the input or extending past the
   fixed buffer when the original extension is shorter than the replacement. */
static bool cue_path(char *out,size_t capacity,const char *path,const char *suffix) {
  const char *dot=strrchr(path,'.');
  const char *slash=strrchr(path,'/'),*backslash=strrchr(path,'\\');
  if(!dot || (slash&&dot<slash) || (backslash&&dot<backslash))return false;
  size_t prefix=(size_t)(dot-path),tail=strlen(suffix);
  if(prefix>=capacity || tail>=capacity-prefix)return false;
  memcpy(out,path,prefix);memcpy(out+prefix,suffix,tail+1);return true;
}
/* Multi-event frames occur in the original film (e.g. two paper sounds
   at once). Keep legacy .sfx compatibility, prefer checked SFX2 sidecars. */
static bool load_cues_v2(LumeAnimation *a, const char *path) {
  char name[160];
  if(!cue_path(name,sizeof name,path,".sf2"))return false;
  FILE *f = fopen(name, "rb");
  if (!f) return false;
  uint8_t h[12];
  bool ok = false;
  uint32_t *offsets = NULL;
  uint16_t *ids = NULL;
  unsigned count = 0;
  if (fread(h, 1, 12, f) != 12 || memcmp(h, "SFX2", 4) ||
      le(h + 4) != a->frames || (count = le(h + 8)) > 65536)
    goto done;
  offsets = calloc(a->frames + 1, sizeof(*offsets));
  ids = calloc(count ? count : 1, sizeof(*ids));
  if (!offsets || !ids) goto done;
  if(fread(offsets,4,a->frames+1,f)!=a->frames+1)goto done;
  for(unsigned i=0;i<=a->frames;i++) {
    offsets[i]=le((const uint8_t*)&offsets[i]);
    if(offsets[i]>count || (!i && offsets[i]) ||
       (i && offsets[i]<offsets[i-1]))goto done;
  }
  if(offsets[a->frames]!=count)goto done;
  if(fread(ids,2,count,f)!=count)goto done;
  for(unsigned i=0;i<count;i++) {
    const uint8_t *p=(const uint8_t*)&ids[i];ids[i]=p[0]|(uint16_t)p[1]<<8;
  }
  if (fgetc(f) != EOF) goto done;
  a->cue_offsets = offsets;
  a->cue_ids = ids;
  a->cue_count = count;
  ok = true;
done:
  fclose(f);
  if (!ok) { free(offsets); free(ids); }
  return ok;
}
bool lume_animation_open(LumeAnimation *a, const char *path) {
  memset(a, 0, sizeof(*a));
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  uint8_t h[20];
  if (fread(h, 1, 20, f) != 20 || (memcmp(h,"LAN2",4) && memcmp(h,"LAN3",4) && memcmp(h,"LAN4",4) && memcmp(h,"LAN5",4)) || le(h + 4) != 320 ||
      le(h + 8) != 240) {
    fclose(f);
    return false;
  }
  a->temporal_codec=!memcmp(h,"LAN5",4);
  a->spatial_codec=!memcmp(h,"LAN4",4)||a->temporal_codec;
  a->mixed_codec=!memcmp(h,"LAN3",4)||a->spatial_codec;
  a->fps = le(h + 12);
  a->frames = le(h + 16);
  if (a->fps < 1 || a->fps > 60 || !a->frames || a->frames > 5000) {
    fclose(f);
    return false;
  }
  a->file = f;
  a->offsets = calloc(a->frames + 1, 4);
  if (!a->offsets) {
    lume_animation_close(a);
    return false;
  }
  /* Read contiguous index tables once, then retain all bounds checks. */
  if(fread(a->offsets,4,a->frames+1,f)!=a->frames+1) {
    lume_animation_close(a);return false;
  }
  for(unsigned i=0;i<=a->frames;i++) {
    a->offsets[i]=le((const uint8_t*)&a->offsets[i]);
    if(i && (a->offsets[i]<=a->offsets[i-1] ||
       a->offsets[i]-a->offsets[i-1]>1024*1024)) {
      lume_animation_close(a);return false;
    }
  }
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  if (size < 0 || a->offsets[0] != 20 + 4 * (a->frames + 1) ||
      a->offsets[a->frames] != (uint32_t)size) {
    lume_animation_close(a);
    return false;
  }
  for (unsigned i = 0; i < a->frames; i++) {
    size_t n = a->offsets[i + 1] - a->offsets[i];
    if (n > a->packet_capacity)
      a->packet_capacity = n;
  }
  if (!lume_cache_init(&a->cache, f, size)) {
    lume_animation_close(a);
    return false;
  }
  a->file_size=(uint32_t)size;
  a->lan_state.prepared=~0u;
  a->packet = NULL;
  a->rgba = malloc(320 * 240 * 4);
  a->prepared = ~0u;
  if (!a->rgba) {
    lume_animation_close(a);
    return false;
  }
  if (load_cues_v2(a, path))
    return true;
  char cuepath[160];
  if(!cue_path(cuepath,sizeof cuepath,path,".sfx"))return true;
  FILE *c = fopen(cuepath, "rb");
  if (c) {
    a->cues = calloc(a->frames, 2);
    if (a->cues && fread(a->cues, 2, a->frames, c) != a->frames) {
      free(a->cues);
      a->cues = NULL;
    }
    fclose(c);
  }
  return true;
}
bool lume_animation_frame(LumeAnimation *a, unsigned frame, uint8_t *rgb,
                          int eye) {
  if (!a->file || frame >= a->frames)
    return false;
  if (frame != a->prepared) {
    LumeLanInput input={.file=a->file,.cache=&a->cache,.offsets=a->offsets,
      .frames=a->frames,.width=320,.height=240,.total=a->file_size,
      .mixed=a->mixed_codec,.spatial=a->spatial_codec,.temporal=a->temporal_codec};
    LumeLanRecord record;unsigned fast_before=a->lan_state.lz4_blocks;
    if(!lume_lan_decode(&input,&a->lan_state,frame,a->rgba,320*240*4,&record)){a->prepared=~0u;return false;}
    a->reads++;a->decodes++;
    a->fast_decodes+=a->lan_state.lz4_blocks-fast_before;
    a->x=record.x;a->y=record.y;a->width=record.width;a->height=record.height;
    a->movie=record.background>>16;a->background=record.background&65535;
    a->prepared = frame;
  }
  if (rgb && a->width && a->height)
    lume_blit_rgba_eye(rgb,a->rgba,a->width,a->height,a->x,a->y,eye);
  return true;
}
/* Read only checked metadata for the actual next authored pose. Do not
   guess frame+1 in the film: navigation may hold or jump camera frames. */
bool lume_animation_camera_at(LumeAnimation *a,unsigned frame,unsigned *movie,unsigned *background) {
 if(!a->file||frame>=a->frames)return false;
 LumeLanInput input={.file=a->file,.cache=&a->cache,.offsets=a->offsets,
  .frames=a->frames,.width=320,.height=240,.total=a->file_size,
  .mixed=a->mixed_codec,.spatial=a->spatial_codec,.temporal=a->temporal_codec};
 LumeLanRecord record;if(!lume_lan_record(&input,frame,&record))return false;
 *movie=record.background>>16;*background=record.background&65535;return true;
}
bool lume_animation_camera_range(LumeAnimation *a, unsigned *movie,
                                 unsigned *first, unsigned *last) {
  unsigned mid = 4, low = ~0u, high = 0;
  for (unsigned i = 0; i < a->frames; i++) {
    const uint8_t *p = lume_cache_get(&a->cache, a->offsets[i], 12);
    if (!p)
      return false;
    unsigned packed = le(p);
    if(a->mixed_codec)packed&=a->temporal_codec?0x03ffffffu:a->spatial_codec?0x0fffffffu:0x7fffffffu;
    unsigned m = packed >> 16, fr = packed & 65535;
    if (mid == 4 && m < 4)
      mid = m;
    if (m == mid && mid < 4) {
      if (fr < low)
        low = fr;
      if (fr > high)
        high = fr;
    }
  }
  if (mid == 4)
    return false;
  *movie = mid;
  *first = low;
  *last = high;
  return true;
}
