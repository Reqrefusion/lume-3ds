/* 3DS framebuffer rotation, 400x240 staging and double-buffer tags.
   Scalar implementation also runs in the host reference harness. */
/* @hotpath Tile the transpose to keep both source and destination in ARM11 L1.
 */
static void rotate_bgr(const uint8_t *rgb, uint8_t *fb) {
  for (int ty = 0; ty < 240; ty += 8)
    for (int tx = 0; tx < 320; tx += 8)
      for (int y = ty; y < ty + 8; y++) {
        const uint8_t *s = rgb + (y * 320 + tx) * 3;
        uint8_t *d = fb + (tx * 240 + 239 - y) * 3;
        for (int x = 0; x < 8; x++, s += 3, d += 720) {
          d[0] = s[2];
          d[1] = s[1];
          d[2] = s[0];
        }
      }
}
/* Source pixels are centred samples. Nearest-neighbour enlargement repeats
 * uneven rows/columns, making paper lettering and fine wires visibly jagged.
 * Two separable integer passes interpolate the original RGB values. Keep ten
 * horizontal rows: one eight-row output tile needs at most nine source rows.
 * No allocation, floating point, sharpening halo or new framebuffer per eye. */
typedef struct { uint16_t first, second, weight; } StageSample;
static void stage_axis(StageSample *out,unsigned source,unsigned dest,unsigned start) {
  for(unsigned i=0;i<dest;i++) {
    int pos=(int)(((2*i+1)*source*256+dest)/(2*dest))-128;
    int base=pos>=0?pos/256:-((-pos+255)/256);
    unsigned weight=pos-base*256;
    // Ease between pixel centres: preserve narrow ink strokes instead of
    // softening every fractional sample with a uniform linear weight.
    weight=(weight*weight*(768-2*weight)+32768)>>16;
    if(base<0){base=0;weight=0;}
    if((unsigned)base>=source-1){base=source-1;weight=0;}
    out[i].first=start+base;
    out[i].second=start+((unsigned)base+1<source?(unsigned)base+1:(unsigned)base);
    out[i].weight=weight;
  }
}
/* R/B occupy separate 16-bit lanes. Their weighted sums fit in those lanes;
 * the green channel is blended separately. RGB channels never mix together. */
static uint32_t stage_mix(uint32_t a,uint32_t b,unsigned weight) {
  unsigned inv=256-weight;
  uint32_t rb=(((a&0xff00ffu)*inv+(b&0xff00ffu)*weight+0x800080u)>>8)&0xff00ffu;
  uint32_t g=((((a>>8)&255)*inv+((b>>8)&255)*weight+128)>>8)<<8;
  return rb|g;
}
static uint32_t stage_pixel(const uint8_t *s) {
  return s[0]|(uint32_t)s[1]<<8|(uint32_t)s[2]<<16;
}
static StageSample stage_columns[360],stage_rows[2][240];
static void stage_prepare(void) {
  static bool ready;
  if(!ready) {
    stage_axis(stage_columns,320,360,0);
    stage_axis(stage_rows[0],LUME_MEDIA_BOTTOM-LUME_MEDIA_TOP,240,LUME_MEDIA_TOP);
    stage_axis(stage_rows[1],LUME_MEDIA_BOTTOM-LUME_MEDIA_TOP+(LUME_MEDIA_BOTTOM<240),240,LUME_MEDIA_TOP);
    ready=true;
  }
}
static void stage_borders(uint8_t *fb) {
  memset(fb,0,20*240*3);memset(fb+380*240*3,0,20*240*3);
}
/* Immutable sample tables are prepared by the caller before submitting work.
   The reusable row window belongs to this invocation, never to another eye
   or worker. Half-frame boundaries are aligned to the existing eight-row tile. */
static void top_stage_range(const uint8_t *rgb,uint8_t *fb,unsigned extended,
                            unsigned first,unsigned last) {
  uint32_t horizontal[10][360],packed[320];
  unsigned tags[10];for(unsigned i=0;i<10;i++)tags[i]=~0u;
  for(unsigned ty=first;ty<last;ty+=8) {
    for(unsigned y=ty;y<ty+8;y++) {
      StageSample v=stage_rows[extended][y];unsigned needed[2]={v.first,v.second};
      for(unsigned k=0;k<2;k++) {
        unsigned source=needed[k],slot=source%10;
        if(tags[slot]==source)continue;
        const uint8_t *scan=rgb+source*320*3;
        /* Pack every source pixel once. The horizontal enlargement samples
           neighbouring pixels repeatedly; reuse 32-bit colours instead of
           reloading/packing both RGB triplets for every output column. */
        for(unsigned x=0;x<320;x++)packed[x]=stage_pixel(scan+x*3);
        for(unsigned x=0;x<360;x++) {
          StageSample u=stage_columns[x];
          horizontal[slot][x]=u.weight?stage_mix(packed[u.first],packed[u.second],u.weight):packed[u.first];
        }
        tags[slot]=source;
      }
    }
    for(unsigned tx=0;tx<360;tx+=8)
      for(unsigned y=ty;y<ty+8;y++) {
        StageSample v=stage_rows[extended][y];
        const uint32_t *a=horizontal[v.first%10],*b=horizontal[v.second%10];
        uint8_t *d=fb+((tx+20)*240+239-y)*3;
        for(unsigned x=tx;x<tx+8;x++,d+=720) {
          uint32_t c=stage_mix(a[x],b[x],v.weight);
          d[0]=c>>16;d[1]=c>>8;d[2]=c;
        }
      }
  }
}
/* The camera often holds while a small authored character crop changes.
   Retain the exact filtered eye, compare source row bands, then recompute only
   affected output bands. Framebuffer copies cover physical double-buffer
   changes; cached pixels never include later UI/card overlays. Each eye owns
   its history, so the parallel jobs do not share writable state. */
typedef struct {
 uint8_t source[320*240*3],filtered[400*240*3];
 unsigned extended,full,partial,reused,saved_rows;
 bool valid;
} LumeStageHistory;
static LumeStageHistory stage_history[2];
static void top_stage_cached(const uint8_t *rgb,uint8_t *fb,unsigned extended,unsigned eye) {
 LumeStageHistory *history=&stage_history[eye];
 uint32_t changed=0;
 if(!history->valid||history->extended!=extended)changed=0x3fffffffu;
 else for(unsigned tile=0;tile<30;tile++) {
  unsigned y=tile*8;
  unsigned first=stage_rows[extended][y].first;
  unsigned last=stage_rows[extended][y+7].second+1;
  if(memcmp(rgb+first*960,history->source+first*960,(last-first)*960))changed|=1u<<tile;
 }
 if(changed==0x3fffffffu) {
  stage_borders(fb);top_stage_range(rgb,fb,extended,0,240);history->full++;
 }else {
  memcpy(fb,history->filtered,sizeof history->filtered);
  unsigned rows=0;
  for(unsigned tile=0;tile<30;) {
   if(!(changed&(1u<<tile))){tile++;continue;}
   unsigned first=tile++;
   while(tile<30&&(changed&(1u<<tile)))tile++;
   top_stage_range(rgb,fb,extended,first*8,tile*8);rows+=(tile-first)*8;
  }
  history->saved_rows+=240-rows;
  if(changed)history->partial++;else history->reused++;
 }
 if(changed||!history->valid) {
  memcpy(history->source,rgb,sizeof history->source);
  memcpy(history->filtered,fb,sizeof history->filtered);
  history->extended=extended;history->valid=true;
 }
}
void lume_present_filter_stats(unsigned *full,unsigned *partial,unsigned *reused,unsigned *saved_rows) {
 *full=stage_history[0].full+stage_history[1].full;
 *partial=stage_history[0].partial+stage_history[1].partial;
 *reused=stage_history[0].reused+stage_history[1].reused;
 *saved_rows=stage_history[0].saved_rows+stage_history[1].saved_rows;
}
typedef struct {const uint8_t *rgb;uint8_t *fb;unsigned extended,eye;} LumeStageJob;
static void stage_job(void *arg) {
 const LumeStageJob *j=arg;top_stage_cached(j->rgb,j->fb,j->extended,j->eye);
}
static bool top_current(const LumePresentCache *cache,uint8_t *buffer,unsigned eye,unsigned generation) {
  if(!cache)return false;
  for(unsigned i=0;i<4;i++)if(cache->tags[i].valid && cache->tags[i].buffer==buffer &&
    cache->tags[i].eye==eye && cache->tags[i].generation==generation)return true;
  return false;
}
static void top_record(LumePresentCache *cache,uint8_t *buffer,unsigned eye,unsigned generation) {
  if(!cache)return;
  unsigned slot=cache->next++%4;
  for(unsigned i=0;i<4;i++)if(cache->tags[i].buffer==buffer){slot=i;break;}
  for(unsigned i=0;i<4;i++)if(cache->tags[i].buffer==buffer)cache->tags[i].valid=false;
  cache->tags[slot].buffer=buffer;cache->tags[slot].eye=eye;
  cache->tags[slot].generation=generation;cache->tags[slot].valid=true;
}
unsigned lume_present_views_parallel_cached(LumePresentCache *cache,unsigned generation,
                      const uint8_t *left,const uint8_t *right,const uint8_t *bottom_rgb,
                      uint8_t *bottom,uint8_t *top_left,uint8_t *top_right,LumeMovieJobs *jobs) {
  rotate_bgr(bottom_rgb,bottom);
  unsigned dirty=LUME_BOTTOM;
  bool need_left=!top_current(cache,top_left,0,generation);
  bool need_right=!top_current(cache,top_right,1,generation);
  if(top_left==top_right && need_left)need_right=true;
  if(!need_left && !need_right)return dirty;
  unsigned extended=0;
  for(unsigned i=LUME_MEDIA_BOTTOM*320*3;i<(LUME_MEDIA_BOTTOM<240?LUME_MEDIA_BOTTOM+1:LUME_MEDIA_BOTTOM)*320*3;i++)
    if(left[i] || right[i]){extended=1;break;}
  stage_prepare();
  bool pair=need_left&&need_right&&left!=right&&top_left!=top_right;
  LumeStageJob other={right,top_right,extended,1};
  bool parallel=false;
  if(pair)parallel=lume_jobs_start(jobs,stage_job,&other);
  if(need_left) {
    top_stage_cached(left,top_left,extended,0);
    if(cache)cache->filtered++;
    top_record(cache,top_left,0,generation);dirty|=LUME_TOP_LEFT;
  }
  if(need_right) {
    if(parallel)lume_jobs_wait(jobs);
    else if(left==right) {
      if(top_right!=top_left)memcpy(top_right,top_left,400*240*3);
    }else top_stage_cached(right,top_right,extended,1);
    if(cache&&left!=right)cache->filtered++;
    top_record(cache,top_right,1,generation);dirty|=LUME_TOP_RIGHT;
  }
  return dirty;
}
unsigned lume_present_views_cached(LumePresentCache *cache,unsigned generation,
                      const uint8_t *left,const uint8_t *right,const uint8_t *bottom_rgb,
                      uint8_t *bottom,uint8_t *top_left,uint8_t *top_right) {
  return lume_present_views_parallel_cached(cache,generation,left,right,bottom_rgb,
                                            bottom,top_left,top_right,NULL);
}
void lume_present_views(const uint8_t *left,const uint8_t *right,const uint8_t *bottom_rgb,
                        uint8_t *bottom,uint8_t *top_left,uint8_t *top_right) {
  lume_present_views_cached(NULL,0,left,right,bottom_rgb,bottom,top_left,top_right);
}
void lume_present_rgb(const uint8_t *left,const uint8_t *right,
                      uint8_t *bottom,uint8_t *top_left,uint8_t *top_right) {
  lume_present_views(left,right,left,bottom,top_left,top_right);
}

void lume_present_bottom(const uint8_t *rgb,uint8_t *fb){rotate_bgr(rgb,fb);}
