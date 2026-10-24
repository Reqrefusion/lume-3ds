#ifndef LUME_VIDEO_CACHE_H
#define LUME_VIDEO_CACHE_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
typedef struct { const void *address; unsigned serial; bool valid; } LumeVideoBuffer;
typedef struct {
 bool valid,stereo; unsigned serial; const void *left,*right;
 LumeVideoBuffer buffers[2][2]; unsigned next[2];
} LumeVideoCache;
static inline void lume_video_cache_reset(LumeVideoCache *c){memset(c,0,sizeof *c);}
static inline bool lume_video_cache_matches(const LumeVideoCache *c,unsigned serial,const void *l,const void *r){
 return c->valid&&c->serial==serial&&c->left==l&&c->right==r;
}
static inline void lume_video_cache_commit(LumeVideoCache *c,unsigned serial,const void *l,const void *r){
 c->valid=true;c->serial=serial;c->left=l;c->right=r;c->stereo=l!=r;
 /* Serial wrap and stereo changes cannot resurrect old physical frames. */
 memset(c->buffers,0,sizeof c->buffers);
}
static inline bool lume_video_buffer_matches(const LumeVideoCache *c,unsigned eye,const void *fb){
 for(unsigned i=0;i<2;i++)if(c->buffers[eye][i].valid&&c->buffers[eye][i].address==fb&&c->buffers[eye][i].serial==c->serial)return true;
 return false;
}
static inline void lume_video_buffer_commit(LumeVideoCache *c,unsigned eye,const void *fb){
 unsigned i=c->next[eye]++%2;c->buffers[eye][i]=(LumeVideoBuffer){fb,c->serial,true};
}
#endif
