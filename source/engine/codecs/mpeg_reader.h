#ifndef LUME_MPEG_READER_H
#define LUME_MPEG_READER_H
#include "engine/codecs/movie.h"
#include "engine/core/layout.h"
#include "engine/codecs/pixel_predict.h"
#include "engine/render/disparity.h"
#include <lz4.h>
#include <stdlib.h>
#include <string.h>
#include "engine/core/jobs.h"
#if defined(LUME_HOST_JOBS)
#include <time.h>
#endif
/* Only this translation unit instantiates the MIT-licensed MPEG decoder.
   UI, source frame numbers and sound events remain outside the video stream. */
#define PLM_NO_STDIO
#define PL_MPEG_IMPLEMENTATION
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "engine/codecs/pl_mpeg.h"
#pragma GCC diagnostic pop

#include "engine/codecs/mpeg_colour_tables.h"
/* Preserve PL_MPEG's integer BT.601 coefficients and rounding. Only the
   photographed rows need conversion; the padded rows are exact black. */
#define LUME_MPEG_PUT_PIXEL(Y_OFFSET,DEST_OFFSET) \
 y=lume_colour_y[frame->y.data[y_index+(Y_OFFSET)]]; \
 dest[d_index+(DEST_OFFSET)]=plm_clamp(y+r); \
 dest[d_index+(DEST_OFFSET)+1]=plm_clamp(y-g); \
 dest[d_index+(DEST_OFFSET)+2]=plm_clamp(y+b)
static void lume_mpeg_rgb(plm_frame_t *frame,uint8_t *dest){
 const int stride=320*3,yw=frame->y.width,cw=frame->cb.width;
 memset(dest,0,LUME_MEDIA_TOP*stride);
 memset(dest+LUME_MEDIA_BOTTOM*stride,0,(240-LUME_MEDIA_BOTTOM)*stride);
 int c_index,y_index,d_index;
 for(int row=LUME_MEDIA_TOP;row<LUME_MEDIA_BOTTOM;) {
  c_index=(row/2)*cw;y_index=row*yw;d_index=row*stride;
  bool pair=!(row&1)&&row+1<LUME_MEDIA_BOTTOM;
  for(int col=0;col<160;col++,c_index++,y_index+=2,d_index+=6){
   int y,cr=frame->cr.data[c_index],cb=frame->cb.data[c_index];
   int r=lume_colour_r[cr],g=(lume_colour_gu[cb]+lume_colour_gv[cr])>>16,b=lume_colour_b[cb];
   LUME_MPEG_PUT_PIXEL(0,0);LUME_MPEG_PUT_PIXEL(1,3);
   if(pair){LUME_MPEG_PUT_PIXEL(yw,stride);LUME_MPEG_PUT_PIXEL(yw+1,stride+3);}
  }
  row+=pair?2:1;
 }
}
#undef LUME_MPEG_PUT_PIXEL
#include "engine/platform/3ds/y2r.h"
#include "engine/platform/clock.h"
static uint64_t lume_mpeg_ticks(void){return lume_platform_ticks();}

#define LUME_MPEG_RGB_BYTES (320u*240u*3u)
typedef struct {
 plm_video_t *decoder;
 LumeMovieJobs *jobs;
 LumeY2R y2r;
 unsigned colour_samples;uint64_t colour_hw_ticks,colour_sw_ticks;
 bool colour_selected_software;
 bool job_ok;unsigned job_kind,job_frame,job_flags,job_size;
 uint64_t job_ticks;
 uint8_t *job_data;size_t job_capacity;
 unsigned first,count,prepared,depth_prepared;
 uint8_t *right_cache;
 bool right_valid;unsigned strength;
 /* DMA cache invalidation must never touch neighbouring decoder fields.
    Align inside the owned allocation without another heap allocation. */
 uint8_t rgb_storage[LUME_MPEG_RGB_BYTES+31],*rgb;
 uint8_t depth[320*240],residual[320*240];
} LumeMpegState;
typedef struct {unsigned first,count,flags,colour,depth;} LumeMpegPacket;
static void lume_mpeg_colour(LumeMovie *m,plm_frame_t *picture,LumeMpegState *s) {
 bool warm=s->y2r.checked,software_ready=false;
 uint64_t begin=lume_mpeg_ticks();
 bool hw=lume_y2r_frame(&s->y2r,picture,s->rgb);
 uint64_t hw_ticks=lume_mpeg_ticks()-begin;
 m->stats.y2r_failure=s->y2r.failure;m->stats.y2r_result=s->y2r.result;
 if(hw&&(!s->y2r.checked||s->colour_samples<3)) {
  bool first=!s->y2r.checked;s->y2r.checked=true;
  uint8_t *reference=malloc(LUME_MPEG_RGB_BYTES);
  if(reference) {
   uint64_t sw_begin=lume_mpeg_ticks();
   lume_mpeg_rgb(picture,reference);
   uint64_t sw_ticks=lume_mpeg_ticks()-sw_begin;
   if(first) {
    unsigned sum=0,worst=0;
    for(unsigned i=LUME_MEDIA_TOP*320*3;i<LUME_MEDIA_BOTTOM*320*3;i++) {
     unsigned d=reference[i]>s->rgb[i]?reference[i]-s->rgb[i]:s->rgb[i]-reference[i];
     sum+=d;if(d>worst)worst=d;
    }
    m->stats.y2r_mae_milli=(uint64_t)sum*1000/(((LUME_MEDIA_BOTTOM-LUME_MEDIA_TOP)*320*3));
    hw=worst<=8 && sum<=((LUME_MEDIA_BOTTOM-LUME_MEDIA_TOP)*320*3)*3/2;
   }
   /* Ignore first-use service initialization. Compare three warmed frames;
      allocation and the colour comparison are outside the timed regions. */
   if(hw&&warm&&hw_ticks&&sw_ticks) {
    s->colour_hw_ticks+=hw_ticks;s->colour_sw_ticks+=sw_ticks;s->colour_samples++;
    m->stats.colour_hw_ticks=s->colour_hw_ticks;m->stats.colour_sw_ticks=s->colour_sw_ticks;
    m->stats.colour_samples=s->colour_samples;
    if(s->colour_samples==3&&s->colour_hw_ticks>s->colour_sw_ticks+s->colour_sw_ticks/8) {
     s->colour_selected_software=true;s->y2r.disabled=true;
     s->y2r.failure="software_faster";s->y2r.result=0;
     lume_y2r_close(&s->y2r);hw=false;
    }
   }
   if(!hw){memcpy(s->rgb,reference,LUME_MPEG_RGB_BYTES);software_ready=true;}
   free(reference);
  }else if(first)hw=false;
  else s->colour_samples=3; /* No repeated allocation attempts under pressure. */
  if(!hw&&!s->colour_selected_software) {
   s->y2r.failure="colour_check";s->y2r.result=-1;
   s->y2r.disabled=true;lume_y2r_close(&s->y2r);
  }
  m->stats.y2r_failure=s->y2r.failure;m->stats.y2r_result=s->y2r.result;
 }
 if(hw){m->stats.y2r_frames++;if(s->y2r.direct)m->stats.y2r_direct_frames++;}
 else {
  if(s->y2r.checked||s->y2r.disabled)m->stats.y2r_fallbacks++;
  if(!software_ready)lume_mpeg_rgb(picture,s->rgb);
 }
}
static uint32_t lume_mpeg_le(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void lume_mpeg_reset(LumeMpegState *s){
 lume_jobs_wait(s->jobs);
 if(s->decoder)plm_video_destroy(s->decoder);
 s->decoder=NULL;s->first=s->prepared=s->depth_prepared=~0u;s->count=0;s->right_valid=false;
}
static bool lume_mpeg_init(LumeMovie *m){
 LumeMpegState *s=calloc(1,sizeof *s);
 if(!s)return false;
 s->rgb=(uint8_t*)(((uintptr_t)s->rgb_storage+31)&~(uintptr_t)31);
 s->first=s->prepared=s->depth_prepared=~0u;s->jobs=lume_jobs_create();m->stats.worker_core=s->jobs?s->jobs->core:-1;m->mpeg_state=s;return true;
}
static void lume_mpeg_close(LumeMovie *m){
 LumeMpegState *s=m->mpeg_state;
 if(s){lume_mpeg_reset(s);lume_jobs_close(s->jobs);free(s->job_data);free(s->right_cache);lume_y2r_close(&s->y2r);free(s);m->mpeg_state=NULL;}
}
static bool lume_mpeg_packet(LumeMovie *m,unsigned frame,LumeMpegPacket *p,const uint8_t **data){
 if(frame>=m->frames)return false;
 size_t n=m->offsets[frame+1]-m->offsets[frame];
 if(n<16 || n>1024*1024)return false;
 const uint8_t *b=lume_cache_get(&m->cache,m->offsets[frame],n);
 if(!b)return false;
 p->first=lume_mpeg_le(b);p->count=b[4]|(unsigned)b[5]<<8;
 p->flags=b[6]|(unsigned)b[7]<<8;p->colour=lume_mpeg_le(b+8);p->depth=lume_mpeg_le(b+12);
 if(!p->count || p->count>m->mpeg_group_limit || p->first>frame ||
    p->first>=m->frames || p->count>m->frames-p->first || frame-p->first>=p->count ||
    (p->flags&~0x107u) || p->colour>512*1024 || p->depth>80000 ||
    (uint64_t)16+p->colour+p->depth!=n ||
    (frame!=p->first && p->colour) ||
    ((p->flags>>8) && (p->first!=frame || p->count!=1)))return false;
 *data=b+16;return true;
}
static bool lume_mpeg_anchor(LumeMovie *m,unsigned frame,unsigned *first){
 LumeMpegPacket p;const uint8_t *data;
 if(!lume_mpeg_packet(m,frame,&p,&data))return false;
 *first=p.first;return true;
}
static bool lume_mpeg_start(LumeMovie *m,unsigned first,unsigned count){
 LumeMpegState *s=m->mpeg_state;LumeMpegPacket p;const uint8_t *data;
 lume_mpeg_reset(s);
 if(!lume_mpeg_packet(m,first,&p,&data) || p.first!=first || p.count!=count || !p.colour)return false;
 if(!(p.flags>>8)){
  /* Own the elementary group: cache prefetch or another packet read may
     replace its source bytes while the MPEG reference pictures are alive. */
  uint8_t *owned=malloc(p.colour);
  if(!owned)return false;
  memcpy(owned,data,p.colour);
  plm_buffer_t *buffer=plm_buffer_create_with_memory(owned,p.colour,1);
  if(!buffer){free(owned);return false;}
  s->decoder=plm_video_create_with_buffer(buffer,1);
  if(!s->decoder){plm_buffer_destroy(buffer);return false;}
  if(plm_video_get_width(s->decoder)!=320 || plm_video_get_height(s->decoder)!=240 ||
     plm_video_get_framerate(s->decoder)!=25.0){lume_mpeg_reset(s);return false;}
  plm_video_set_no_delay(s->decoder,1);
 }
 s->first=first;s->count=count;s->prepared=s->depth_prepared=~0u;memset(s->depth,0,sizeof s->depth);return true;
}
static bool lume_mpeg_depth(LumeMpegState *s,unsigned frame,unsigned flags,
                            const uint8_t *data,unsigned size){
 unsigned mode=flags&7u;
 if(mode==0){if(size)return false;memset(s->depth,0,sizeof s->depth);return true;}
 if(mode==1)return frame>s->first && !size;
 bool temporal=mode>=5;
 if(temporal && frame==s->first)return false;
 uint8_t *dst=temporal?s->residual:s->depth;
 if(LZ4_decompress_safe((const char*)data,(char*)dst,(int)size,320*240)!=320*240 ||
    !lume_pixel_restore(dst,320,240,1,(mode-2)%3))return false;
 if(temporal)for(unsigned i=0;i<320*240;i++)s->depth[i]=(uint8_t)(s->depth[i]+dst[i]);
 return true;
}
/* Mono playback has no visible disparity consumer. Restore just the current
   group's maps when stereo is re-enabled, without replaying MPEG pictures. */
static bool lume_mpeg_ensure_depth(LumeMovie *m,unsigned frame){
 LumeMpegState *s=m->mpeg_state;
 if(s->depth_prepared==frame)return true;
 unsigned next;
 if(s->depth_prepared==~0u || s->depth_prepared>frame || s->depth_prepared<s->first){
  next=s->first;s->depth_prepared=~0u;memset(s->depth,0,sizeof s->depth);
 }else next=s->depth_prepared+1;
 for(unsigned i=next;i<=frame;i++){
  LumeMpegPacket p;const uint8_t *data;
  if(!lume_mpeg_packet(m,i,&p,&data) || p.first!=s->first || p.count!=s->count)return false;
  uint64_t started=lume_mpeg_ticks();bool ok=lume_mpeg_depth(s,i,p.flags,data+p.colour,p.depth);
  m->stats.depth_ticks+=lume_mpeg_ticks()-started;if(!ok)return false;
  s->depth_prepared=i;m->stats.depth_frames++;
 }
 return true;
}
static void lume_mpeg_job(void *arg){
 LumeMpegState *s=arg;uint64_t started=lume_mpeg_ticks();
 if(s->job_kind==0)s->job_ok=lume_mpeg_depth(s,s->job_frame,s->job_flags,s->job_data,s->job_size);
 else {lume_puzzle_reproject_strength_rows(s->rgb,s->right_cache,(const int8_t*)s->depth,s->strength,120,240);s->job_ok=true;}
 s->job_ticks=lume_mpeg_ticks()-started;
}
static bool lume_mpeg_frame(LumeMovie *m,unsigned frame,uint8_t *left,uint8_t *right){
 LumeMpegState *s=m->mpeg_state;LumeMpegPacket wanted;const uint8_t *data;
 if(!s || !lume_mpeg_packet(m,frame,&wanted,&data))return false;
 if(s->first!=wanted.first || s->count!=wanted.count){
  if(!lume_mpeg_start(m,wanted.first,wanted.count))goto failed;
 }else if(s->prepared!=~0u && frame<s->prepared){
  /* Fixed elementary groups retain their bytes, so a local backward seek
     can rewind the existing decoder instead of reallocating its pictures. */
  if(s->decoder)plm_video_rewind(s->decoder);
  s->prepared=s->depth_prepared=~0u;s->right_valid=false;memset(s->depth,0,sizeof s->depth);m->stats.rewinds++;
 }
 if(s->prepared!=frame){
  s->right_valid=false;
  unsigned next=s->prepared==~0u?s->first:s->prepared+1;
  for(unsigned i=next;i<=frame;i++){
   if(right && i>s->first && s->depth_prepared!=i-1 && !lume_mpeg_ensure_depth(m,i-1))goto failed;
   LumeMpegPacket p;
   if(!lume_mpeg_packet(m,i,&p,&data) || p.first!=s->first || p.count!=s->count ||
      (!!(p.flags>>8))!=(s->decoder==NULL))goto failed;
   bool parallel=false;
   if(right && s->jobs && p.depth){
    /* Copy compressed bytes only on hardware with a worker. Keep the buffer
       across frames; allocation failure falls back to the caller's decoder. */
    if(s->job_capacity<p.depth){
     uint8_t *owned=realloc(s->job_data,p.depth);
     if(owned){s->job_data=owned;s->job_capacity=p.depth;}
    }
    if(s->job_capacity>=p.depth){
     memcpy(s->job_data,data+p.colour,p.depth);s->job_kind=0;s->job_frame=i;s->job_flags=p.flags;s->job_size=p.depth;
     parallel=lume_jobs_start(s->jobs,lume_mpeg_job,s);
     if(parallel)m->stats.depth_jobs++;
    }
   }
   uint64_t started=lume_mpeg_ticks();
   if(p.flags>>8){
    if(LZ4_decompress_safe((const char*)data,(char*)s->rgb,(int)p.colour,(int)LUME_MPEG_RGB_BYTES)!=(int)LUME_MPEG_RGB_BYTES)goto failed;
    m->stats.codec_ticks+=lume_mpeg_ticks()-started;
   }else{
    plm_frame_t *picture=plm_video_decode(s->decoder);
    m->stats.codec_ticks+=lume_mpeg_ticks()-started;
    if(!picture)goto failed;
    if(i==frame){
     started=lume_mpeg_ticks();lume_mpeg_colour(m,picture,s);
     m->stats.convert_ticks+=lume_mpeg_ticks()-started;m->stats.conversions++;
    }
   }
   if(right){
    if(parallel){
     started=lume_mpeg_ticks();lume_jobs_wait(s->jobs);m->stats.depth_wait_ticks+=lume_mpeg_ticks()-started;
     m->stats.depth_ticks+=s->job_ticks;if(!s->job_ok)goto failed;
    }else{
     started=lume_mpeg_ticks();bool ok=lume_mpeg_depth(s,i,p.flags,data+p.colour,p.depth);
     m->stats.depth_ticks+=lume_mpeg_ticks()-started;if(!ok)goto failed;
    }
    s->depth_prepared=i;m->stats.depth_frames++;
   }else m->stats.depth_skipped++;
   m->stats.decoded_frames++;s->prepared=i;
  }
 }else m->stats.cached_frames++;
 memcpy(left,s->rgb,LUME_MPEG_RGB_BYTES);
 if(right){
  if(!lume_mpeg_ensure_depth(m,frame))goto failed;
  if(!s->right_cache)s->right_cache=malloc(LUME_MPEG_RGB_BYTES);
  if(s->right_cache){
   if(!s->right_valid){
    uint64_t started=lume_mpeg_ticks();s->job_kind=1;s->strength=m->stereo_strength;
    bool parallel=lume_jobs_start(s->jobs,lume_mpeg_job,s);
    if(parallel){
     m->stats.stereo_jobs++;lume_puzzle_reproject_strength_rows(s->rgb,s->right_cache,(const int8_t*)s->depth,m->stereo_strength,0,120);
     lume_jobs_wait(s->jobs);
    }else lume_puzzle_reproject_strength(s->rgb,s->right_cache,(const int8_t*)s->depth,m->stereo_strength);
    m->stats.stereo_ticks+=lume_mpeg_ticks()-started;m->stats.stereo_frames++;s->right_valid=true;
   }
   memcpy(right,s->right_cache,LUME_MPEG_RGB_BYTES);
  }else lume_puzzle_reproject_strength(s->rgb,right,(const int8_t*)s->depth,m->stereo_strength);
 }
 m->last=frame;m->last_stereo=right!=NULL;return true;
failed:
 /* A failed colour decode must still join the independent depth work before
    releasing its state, or a late job could write through a freed movie. */
 lume_mpeg_reset(s);m->last=~0u;return false;
}
#endif
