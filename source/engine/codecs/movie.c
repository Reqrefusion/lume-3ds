#include "engine/codecs/movie.h"
#include "engine/codecs/blocks.h"
#include "engine/render/stereo_video.h"
#include <jpeglib.h>
#include <lz4.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include "engine/codecs/mpeg_reader.h"
static bool lume_movie_frame_now(LumeMovie *,unsigned,uint8_t *,uint8_t *);
typedef struct {
 LumeMovieJobs *jobs;LumeMovie *movie;uint8_t *pixels;
 unsigned frame;bool stereo,pending,ok;
} LumeLookahead;
static void lume_lookahead_run(void *arg) {
 LumeLookahead *q=arg;
 q->ok=lume_movie_frame_now(q->movie,q->frame,q->pixels,q->stereo?q->pixels+230400:NULL);
}
bool lume_movie_ready(LumeMovie *m) {
 LumeLookahead *q=m->lookahead;
 bool ready=!q||!q->pending||lume_jobs_ready(q->jobs);
 if(!ready)m->stats.lookahead_busy_polls++;
 return ready;
}
void lume_movie_sync(LumeMovie *m) {
 LumeLookahead *q=m->lookahead;
 if(q&&q->pending)lume_jobs_wait(q->jobs);
}
static void lume_lookahead_close(LumeMovie *m) {
 LumeLookahead *q=m->lookahead;
 if(q){lume_jobs_close(q->jobs);free(q->pixels);free(q);m->lookahead=NULL;}
}
bool lume_movie_hint(LumeMovie *m,unsigned frame,bool stereo) {
 if(!m->file||!m->mpeg||frame>=m->frames)return false;
 LumeLookahead *q=m->lookahead;
 if(q&&q->pending) {
  if(q->frame==frame&&q->stereo==stereo)return true;
  lume_movie_sync(m);q->pending=false;
 }
 if(frame==m->last&&(!stereo||m->last_stereo))return false;
 if(!q) {
  if(m->lookahead_tried)return false;
  m->lookahead_tried=true;
  LumeMovieJobs *jobs=lume_jobs_create();if(!jobs)return false;
  q=calloc(1,sizeof *q);
  if(q)q->pixels=malloc(2*230400);
  if(!q||!q->pixels){if(q)free(q);lume_jobs_close(jobs);return false;}
  q->jobs=jobs;q->movie=m;m->lookahead=q;
 }
 q->frame=frame;q->stereo=stereo;q->ok=false;
 q->pending=lume_jobs_start(q->jobs,lume_lookahead_run,q);
 if(q->pending)m->stats.lookahead_requests++;
 return q->pending;
}
static uint32_t le32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}
void lume_movie_close(LumeMovie *m) {
  lume_lookahead_close(m);
  lume_mpeg_close(m);
  lume_cache_close(&m->cache);
  if (m->file)
    fclose(m->file);
  free(m->offsets);
  free(m->blocks);
  free(m->residual);
  memset(m, 0, sizeof(*m));
}
bool lume_movie_open(LumeMovie *m, const char *path) {
  memset(m, 0, sizeof(*m));
  m->stats.worker_core=-1;m->stereo_strength=256;
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  uint8_t h[20];
  if (fread(h, 1, 20, f) != 20 ||
      (memcmp(h, "LJV1", 4) && memcmp(h, "LVB1", 4) && memcmp(h, "LVS1", 4) && memcmp(h, "LVS2", 4) && memcmp(h, "LVM1", 4)) ||
      !((h[4] == 64 && h[5] == 1 && h[6] == 240 && h[7] == 0) ||
        (h[4] == 0 && h[5] == 1 && h[6] == 192 && h[7] == 0))) {
    fclose(f);
    return false;
  }
  unsigned fps = le32(h + 8), frames = le32(h + 12);
  if (fps < 1 || fps > 60 || frames < 1 || frames > 4000) {
    fclose(f);
    return false;
  }
  uint32_t *offsets = malloc((frames + 1) * sizeof(*offsets));
  if (!offsets) {
    fclose(f);
    return false;
  }
  bool ok=fread(offsets,4,frames+1,f)==frames+1;
  if(ok)for(unsigned i=0;i<=frames;i++) {
    offsets[i]=le32((const uint8_t*)&offsets[i]);
    if(i && (offsets[i]<=offsets[i-1] || offsets[i]-offsets[i-1]>1024*1024))ok=false;
  }
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  if (!ok || offsets[0] != 20 + 4 * (frames + 1) || size < 0 ||
      offsets[frames] != (uint32_t)size) {
    free(offsets);
    fclose(f);
    return false;
  }
  m->mpeg = !memcmp(h,"LVM1",4);
  m->mpeg_group_limit=le32(h+16);
  if(m->mpeg && (fps!=25 || h[4]!=64 || h[5]!=1 || h[6]!=240 || h[7]!=0 ||
                 m->mpeg_group_limit<1 || m->mpeg_group_limit>32)){free(offsets);fclose(f);return false;}
  m->blocked = !memcmp(h, "LVB1", 4);
  m->temporal = !memcmp(h,"LVS2",4);
  m->disparity = !memcmp(h, "LVS1", 4)||m->temporal;
  m->raw_last=~0u;
  if(m->temporal && le32(h+16)!=4){free(offsets);fclose(f);return false;}
  if (m->blocked || m->disparity) {
    m->blocks = malloc(m->disparity ? 230400 : 76800);
    if (!m->blocks) {
      free(offsets);
      fclose(f);
      return false;
    }
  }
  if(m->temporal) {
    m->residual=malloc(230400);
    if(!m->residual){free(m->blocks);free(offsets);fclose(f);memset(m,0,sizeof *m);return false;}
  }
  m->file = f;
  m->fps = fps;
  m->frames = frames;
  m->offsets = offsets;
  if (!lume_cache_init(&m->cache, f, size)) {
    lume_movie_close(m);
    return false;
  }
  if(m->mpeg && !lume_mpeg_init(m)){lume_movie_close(m);return false;}
  m->last = ~0u;
  return true;
}
typedef struct {
  struct jpeg_error_mgr base;
  jmp_buf jump;
} JpegError;
static void jpeg_failure(j_common_ptr c) {
  JpegError *e = (JpegError *)c->err;
  longjmp(e->jump, 1);
}
static uint8_t small_rgb[256 * 192 * 3];
/* @hotpath Fixed integer sampling. UI and authored character layers remain
 * 320x240. */
static __attribute__((noinline)) void upscale_background(const uint8_t *src,
                                                         uint8_t *dst) {
  static uint16_t xmap[320];
  static bool ready;
  if (!ready) {
    for (int x = 0; x < 320; x++)
      xmap[x] = (x * 4 / 5) * 3;
    ready = true;
  }
  int prev = -1;
  for (int y = 0; y < 240; y++) {
    int sy = y * 4 / 5;
    if (sy == prev) {
      memcpy(dst, dst - 320 * 3, 320 * 3);
      dst += 320 * 3;
      continue;
    }
    prev = sy;
    const uint8_t *row = src + sy * 256 * 3;
    for (int x = 0; x < 320; x++) {
      const uint8_t *p = row + xmap[x];
      dst[0] = p[0];
      dst[1] = p[1];
      dst[2] = p[2];
      dst += 3;
    }
  }
}
static bool decode(const uint8_t *bytes, size_t size, uint8_t *rgb) {
  struct jpeg_decompress_struct c;
  JpegError e;
  memset(&c, 0, sizeof(c));
  c.err = jpeg_std_error(&e.base);
  e.base.error_exit = jpeg_failure;
  if (setjmp(e.jump)) {
    jpeg_destroy_decompress(&c);
    return false;
  }
  jpeg_create_decompress(&c);
  jpeg_mem_src(&c, bytes, size);
  jpeg_read_header(&c, TRUE);
  bool small = c.image_width == 256 && c.image_height == 192;
  if (!small && (c.image_width != 320 || c.image_height != 240)) {
    jpeg_destroy_decompress(&c);
    return false;
  }
  c.out_color_space = JCS_RGB;
  c.dct_method = JDCT_IFAST;
  c.do_fancy_upsampling = FALSE;
  c.do_block_smoothing = FALSE;
  jpeg_start_decompress(&c);
  if (c.output_components != 3) {
    jpeg_destroy_decompress(&c);
    return false;
  }
  while (c.output_scanline < c.output_height) {
    JSAMPROW row =
        (small ? small_rgb : rgb) + c.output_scanline * (small ? 256 : 320) * 3;
    jpeg_read_scanlines(&c, &row, 1);
  }
  jpeg_finish_decompress(&c);
  jpeg_destroy_decompress(&c);
  if (small)
    upscale_background(small_rgb, rgb);
  return true;
}
/* Four-frame groups bound cold seeks to at most three predecessors. Only
   RGB565 bytes are predicted; each signed depth plane stays independent.
   Existing streams and the final stereo reconstruction remain unchanged. */
static bool temporal_frame(LumeMovie *m,unsigned frame) {
  if(m->raw_last==frame)return true;
  unsigned first=frame-(frame%4);
  if(m->raw_last!=~0u && m->raw_last<frame && m->raw_last>=first)
    first=m->raw_last+1;
  if(first!=frame) {
    /* Authored clip starts carry independent packets even inside a group.
       Read them directly instead of reconstructing the preceding clip. */
    size_t n=m->offsets[frame+1]-m->offsets[frame];
    const uint8_t *p=lume_cache_get(&m->cache,m->offsets[frame],n);
    if(p && n>=2 && p[0]==0)first=frame;
  }
  for(unsigned i=first;i<=frame;i++) {
    size_t n=m->offsets[i+1]-m->offsets[i];
    const uint8_t *p=lume_cache_get(&m->cache,m->offsets[i],n);
    if(!p || n<2 || p[0]>1 || (!(i%4)&&p[0]) ||
       (p[0] && (m->raw_last==~0u||m->raw_last+1!=i)))goto failed;
    bool delta=p[0]!=0;
    uint8_t *dst=delta?m->residual:m->blocks;
    if(LZ4_decompress_safe((const char*)p+1,(char*)dst,(int)n-1,230400)!=230400)
      goto failed;
    if(delta) {
      for(unsigned j=0;j<76800;j++) {
        m->blocks[2*j]=(uint8_t)(m->blocks[2*j]+dst[j]);
        m->blocks[2*j+1]=(uint8_t)(m->blocks[2*j+1]+dst[76800+j]);
      }
      memcpy(m->blocks+153600,dst+153600,76800);
    }
    m->raw_last=i;
  }
  return true;
failed:
  m->raw_last=~0u;m->last=~0u;return false;
}
static bool lume_movie_frame_now(LumeMovie *m, unsigned frame, uint8_t *left,
                      uint8_t *right) {
  if (!m->file || frame >= m->frames)
    return false;
  if(m->mpeg)return lume_mpeg_frame(m,frame,left,right);
  /* Legacy eye formats must write the requested output buffers even on a
     held frame. The cached packet is reusable; the caller's RGB pointers are
     not guaranteed to be the same pointers as the preceding request. */
  if(m->temporal) {
    if(!temporal_frame(m,frame))return false;
    lume_decode_stereo565_strength(m->blocks,left,right,m->stereo_strength);
    m->last=frame;m->last_stereo=right!=NULL;return true;
  }
  size_t n = m->offsets[frame + 1] - m->offsets[frame];
  const uint8_t *data = lume_cache_get(&m->cache, m->offsets[frame], n);
  if (!data)
    return false;
  if (m->blocked || m->disparity) {
    int bytes = m->disparity ? 230400 : 76800;
    if (LZ4_decompress_safe((const char *)data, (char *)m->blocks, n, bytes) !=
        bytes)
      return false;
    if (m->disparity)
      lume_decode_stereo565_strength(m->blocks,left,right,m->stereo_strength);
    else {
      lume_decode_blocks(m->blocks, left);
      if(right)lume_decode_blocks(m->blocks + 38400, right);
    }
    m->last = frame;
    m->last_stereo=right!=NULL;
    return true;
  }
  bool ok = true;
  if (ok && n < 8)
    ok = false;
  if (ok) {
    uint32_t l = le32(data), r = le32(data + 4);
    ok = n >= 8 && l > 0 && r > 0 && (uint64_t)l + r + 8 == n;
    if (ok)
      ok = decode(data + 8, l, left) && (!right || decode(data + 8 + l, r, right));
  }
  if (ok) {
    m->last = frame;m->last_stereo=right!=NULL;
  }
  return ok;
}
bool lume_movie_frame(LumeMovie *m,unsigned frame,uint8_t *left,uint8_t *right) {
 LumeLookahead *q=m->lookahead;
 if(q&&q->pending) {
  uint64_t started=lume_mpeg_ticks();lume_movie_sync(m);
  m->stats.lookahead_wait_ticks+=lume_mpeg_ticks()-started;q->pending=false;
  if(q->ok&&q->frame==frame&&(!right||q->stereo)) {
   memcpy(left,q->pixels,230400);
   if(right)memcpy(right,q->pixels+230400,230400);
   m->stats.lookahead_hits++;return true;
  }
  m->stats.lookahead_misses++;
 }
 return lume_movie_frame_now(m,frame,left,right);
}
bool lume_movie_prefetch(LumeMovie *m,unsigned first,unsigned last) {
  lume_movie_sync(m);
  if(!m->file || first>last || last>=m->frames)return false;
  if(m->mpeg && !lume_mpeg_anchor(m,first,&first))return false;
  if(m->temporal)first-=first%4;
  return lume_cache_prefetch(&m->cache,m->offsets[first],m->offsets[last+1]-m->offsets[first]);
}
bool lume_movie_prime(LumeMovie *m, unsigned first, unsigned last) {
  lume_movie_sync(m);
  if (first > last || last >= m->frames)
    return false;
  if(m->mpeg && !lume_mpeg_anchor(m,first,&first))return false;
  if(m->temporal)first-=first%4;
  return lume_cache_prime(&m->cache, m->offsets[first],
                          m->offsets[last + 1] - m->offsets[first]);
}

void lume_movie_set_strength(LumeMovie *m,unsigned value) {
 if(value>256)value=256;
 if(m->stereo_strength==value)return;
 /* Drain a future frame before changing the setting it owns. A queued frame
    from the previous setting must never be published as the new right eye. */
 lume_movie_sync(m);
 LumeLookahead *q=m->lookahead;if(q){q->pending=false;q->ok=false;}
 if(m->mpeg_state){LumeMpegState *s=m->mpeg_state;lume_jobs_wait(s->jobs);s->right_valid=false;}
 m->stereo_strength=value;m->last_stereo=false;
}
bool lume_movie_copy_disparity(LumeMovie *m,int8_t *out) {
 lume_movie_sync(m);
 if(m->mpeg_state) {
  LumeMpegState *s=m->mpeg_state;
  if(s->prepared==~0u||!lume_mpeg_ensure_depth(m,s->prepared))return false;
  memcpy(out,s->depth,76800);return true;
 }
 if(m->disparity&&m->blocks&&m->last!=~0u){memcpy(out,m->blocks+153600,76800);return true;}
 return false;
}
const int8_t *lume_movie_disparity_frame(const LumeMovie *m,unsigned frame) {
 if(!m)return NULL;
 const LumeLookahead *q=m->lookahead;
 if(q&&q->pending)return NULL;
 if(m->last!=frame)return NULL;
 if(m->mpeg_state) {
  const LumeMpegState *s=m->mpeg_state;
  return s->prepared==frame&&s->depth_prepared==frame?(const int8_t *)s->depth:NULL;
 }
 return m->disparity&&m->blocks?(const int8_t *)(m->blocks+153600):NULL;
}
