#ifndef LUME_CLIP_COMPOSE_H
#define LUME_CLIP_COMPOSE_H
#include "../../adapters/flash/timeline.h"
#include "../../engine/render/blit.h"
#include "props.h"
#include "../../engine/core/jobs.h"
/* The LAN crop is decoded once before either eye starts. Each eye owns its
   output buffer and only reads the frozen crop/transform table. No work is
   left pending across frame boundaries or scene teardown. */
typedef struct {
 LumeAnimation *clip;const uint8_t *base;uint8_t *out;
 unsigned frame;int turbine,eye;bool props,blades;
 const LumeAnimation *effect;
 int actor_quarter_pixels;
} LumeClipEye;
static inline void lume_clip_eye(void *arg) {
 LumeClipEye *e=arg;
 if(e->out!=e->base)memcpy(e->out,e->base,320*240*3);
 if(e->props)lume_props_frame(e->out,e->frame,e->turbine,e->blades,e->eye);
 if(e->effect&&e->effect->width&&e->effect->height) {
  const LumeAnimation *fx=e->effect;
  lume_blit_rgba_eye(e->out,fx->rgba,fx->width,fx->height,fx->x,fx->y,e->eye);
 }
 LumeAnimation *a=e->clip;
 if(a->width&&a->height)lume_blit_rgba_parallax(e->out,a->rgba,a->width,a->height,a->x,a->y,e->eye,e->actor_quarter_pixels);
}
static inline bool lume_clip_compose_layers(LumeMovieJobs *jobs,LumeAnimation *clip,unsigned frame,
 const uint8_t *left,const uint8_t *right,uint8_t *out_left,uint8_t *out_right,
 bool stereo,bool props,int turbine,bool blades,const LumeAnimation *effect,int actor_quarter_pixels) {
 if(!clip->file||clip->prepared!=frame)return false;
 LumeClipEye l={clip,left,out_left,frame,turbine,0,props,blades,effect,actor_quarter_pixels};
 LumeClipEye r={clip,right,out_right,frame,turbine,2,props,blades,effect,actor_quarter_pixels};
 bool parallel=stereo&&lume_jobs_start(jobs,lume_clip_eye,&r);
 lume_clip_eye(&l);
 if(parallel)lume_jobs_wait(jobs);
 else if(stereo)lume_clip_eye(&r);
 return true;
}
static inline bool lume_clip_compose(LumeMovieJobs *jobs,LumeAnimation *clip,unsigned frame,
 const uint8_t *left,const uint8_t *right,uint8_t *out_left,uint8_t *out_right,
 bool stereo,bool props,int turbine,bool blades,int actor_quarter_pixels) {
 return lume_clip_compose_layers(jobs,clip,frame,left,right,out_left,out_right,
                                 stereo,props,turbine,blades,NULL,actor_quarter_pixels);
}
#endif
