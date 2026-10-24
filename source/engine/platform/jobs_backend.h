#ifndef LUME_MOVIE_JOBS_H
#define LUME_MOVIE_JOBS_H
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "engine/core/jobs_state.h"
/* One persistent, bounded job slot. Codec reference pictures remain on the
   caller; only independent disparity/row work is submitted here. */
#if defined(__3DS__) || defined(LUME_HOST_JOBS)
#ifdef __3DS__
#include <3ds.h>
#include "engine/platform/3ds/math.h"
#else
#include <pthread.h>
#endif
typedef struct {
#ifdef __3DS__
 Thread thread;LightLock lock;LightEvent wake,done;
#else
 pthread_t thread;pthread_mutex_t lock;pthread_cond_t wake,done;
#endif
 void (*run)(void*);void *arg;
 bool pending,stop;int core;
} LumeMovieJobs;
static inline void lume_jobs_lock(LumeMovieJobs *j){
#ifdef __3DS__
 LightLock_Lock(&j->lock);
#else
 pthread_mutex_lock(&j->lock);
#endif
}
static inline void lume_jobs_unlock(LumeMovieJobs *j){
#ifdef __3DS__
 LightLock_Unlock(&j->lock);
#else
 pthread_mutex_unlock(&j->lock);
#endif
}
#ifdef __3DS__
static void lume_jobs_entry(void *arg){
#else
static void *lume_jobs_entry(void *arg){
#endif
 LumeMovieJobs *j=arg;
#ifdef __3DS__
 /* FPSCR is thread-local. These jobs are integer-only, including fallback
    when the floating-point mode cannot be changed. */
 (void)vfp_set_fz_dn();
#endif
 for(;;){
#ifdef __3DS__
  LightEvent_Wait(&j->wake);
  lume_jobs_lock(j);
#else
  lume_jobs_lock(j);
  while(!j->pending && !j->stop)pthread_cond_wait(&j->wake,&j->lock);
#endif
  if(j->stop){lume_jobs_unlock(j);break;}
  void (*run)(void*)=j->run;void *job=j->arg;
  lume_jobs_unlock(j);run(job);lume_jobs_lock(j);j->pending=false;lume_worker_release();
#ifdef __3DS__
  LightEvent_Signal(&j->done);
#else
  pthread_cond_broadcast(&j->done);
#endif
  lume_jobs_unlock(j);
 }
#ifndef __3DS__
 return NULL;
#endif
}
static inline LumeMovieJobs *lume_jobs_create(void){
#ifdef __3DS__
 bool newer=false;
 if(R_FAILED(APT_CheckNew3DS(&newer)) || !newer)return NULL;
 if(lume_runtime_flag("compat_decoder.flag"))return NULL;
#else
 if(getenv("LUME_DISABLE_JOBS"))return NULL;
#endif
 LumeMovieJobs *j=calloc(1,sizeof *j);if(!j)return NULL;j->core=2;
#ifdef __3DS__
 LightLock_Init(&j->lock);LightEvent_Init(&j->wake,RESET_ONESHOT);LightEvent_Init(&j->done,RESET_ONESHOT);
 /* The music worker is higher priority. Never take a second system-core
    slot from Old3DS, and do not assume New3DS core 3 is application-owned. */
 j->thread=threadCreate(lume_jobs_entry,j,32*1024,0x31,2,false);
 if(!j->thread){free(j);return NULL;}
#else
 pthread_mutex_init(&j->lock,NULL);pthread_cond_init(&j->wake,NULL);pthread_cond_init(&j->done,NULL);
 if(pthread_create(&j->thread,NULL,lume_jobs_entry,j)){
  pthread_cond_destroy(&j->done);pthread_cond_destroy(&j->wake);pthread_mutex_destroy(&j->lock);free(j);return NULL;
 }
#endif
 return j;
}
static inline bool lume_jobs_start(LumeMovieJobs *j,void (*run)(void*),void *arg){
 if(!j)return false;
 lume_jobs_lock(j);
 if(j->pending || j->stop || !lume_worker_claim()){lume_jobs_unlock(j);return false;}
 j->run=run;j->arg=arg;j->pending=true;
#ifdef __3DS__
 LightEvent_Clear(&j->done);LightEvent_Signal(&j->wake);
#else
 pthread_cond_signal(&j->wake);
#endif
 lume_jobs_unlock(j);return true;
}
static inline bool lume_jobs_ready(LumeMovieJobs *j){
 if(!j)return true;
 lume_jobs_lock(j);bool ready=!j->pending;lume_jobs_unlock(j);return ready;
}
static inline void lume_jobs_wait(LumeMovieJobs *j){
 if(!j)return;
 lume_jobs_lock(j);
 while(j->pending){
#ifdef __3DS__
  lume_jobs_unlock(j);LightEvent_Wait(&j->done);lume_jobs_lock(j);
#else
  pthread_cond_wait(&j->done,&j->lock);
#endif
 }
 lume_jobs_unlock(j);
}
static inline void lume_jobs_close(LumeMovieJobs *j){
 if(!j)return;
 lume_jobs_wait(j);lume_jobs_lock(j);j->stop=true;
#ifdef __3DS__
 LightEvent_Signal(&j->wake);lume_jobs_unlock(j);threadJoin(j->thread,U64_MAX);threadFree(j->thread);
#else
 pthread_cond_signal(&j->wake);lume_jobs_unlock(j);pthread_join(j->thread,NULL);
 pthread_cond_destroy(&j->done);pthread_cond_destroy(&j->wake);pthread_mutex_destroy(&j->lock);
#endif
 free(j);
}
#else
typedef struct {int core;} LumeMovieJobs;
static inline LumeMovieJobs *lume_jobs_create(void){return NULL;}
static inline bool lume_jobs_start(LumeMovieJobs *j,void (*run)(void*),void *arg){(void)j;(void)run;(void)arg;return false;}
static inline bool lume_jobs_ready(LumeMovieJobs *j){(void)j;return true;}
static inline void lume_jobs_wait(LumeMovieJobs *j){(void)j;}
static inline void lume_jobs_close(LumeMovieJobs *j){(void)j;}
#endif
#endif
