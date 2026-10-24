#ifndef LUME_MOVIE_POOL_H
#define LUME_MOVIE_POOL_H
#include "engine/codecs/movie.h"
/* Slots never move: lookahead jobs retain pointers to their owning movie.
   Only the caller changes the active slot, after joining its previous work. */
typedef struct {struct {LumeMovie clip;int id;} slots[2];unsigned current,hits,misses;} LumeMoviePool;
static inline LumeMovie *lume_movie_pool_get(LumeMoviePool *p,unsigned id,bool (*open)(LumeMovie*,unsigned)) {
 LumeMovie *active=&p->slots[p->current].clip;
 if(active->file)lume_movie_sync(active);
 for(unsigned i=0;i<2;i++)if(p->slots[i].id==(int)id&&p->slots[i].clip.file){p->current=i;p->hits++;return &p->slots[i].clip;}
 unsigned slot=1-p->current;
 if(!p->slots[p->current].clip.file)slot=p->current;
 LumeMovie *target=&p->slots[slot].clip;lume_movie_close(target);p->slots[slot].id=-1;
 if(!open(target,id)){
  lume_movie_close(target);
  /* If the spare decoder cannot be allocated, release the parked camera
     and retry with the former one-camera memory budget. */
  if(active!=target&&active->file){
   lume_movie_close(active);p->slots[p->current].id=-1;
   if(!open(target,id)){lume_movie_close(target);return NULL;}
  }else return NULL;
 }
 p->slots[slot].id=(int)id;p->current=slot;p->misses++;return target;
}
static inline void lume_movie_pool_close(LumeMoviePool *p) {
 for(unsigned i=0;i<2;i++){lume_movie_close(&p->slots[i].clip);p->slots[i].id=-1;}
 p->current=0;
}
#endif
