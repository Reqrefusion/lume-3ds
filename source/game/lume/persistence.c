#include "storage.h"
#include "persistence.h"
#include "audio.h"
#include "engine/core/log_io.h"
#include <string.h>
#ifdef __3DS__
#include <3ds.h>
static LightLock lock;static LightEvent wake;static Thread worker;
#define LOCK() LightLock_Lock(&lock)
#define UNLOCK() LightLock_Unlock(&lock)
#define SIGNAL() LightEvent_Signal(&wake)
#elif defined(LUME_HOST_JOBS)
#include <pthread.h>
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wake=PTHREAD_COND_INITIALIZER;static pthread_t worker;
#define LOCK() pthread_mutex_lock(&lock)
#define UNLOCK() pthread_mutex_unlock(&lock)
#define SIGNAL() pthread_cond_signal(&wake)
#else
#define LOCK() ((void)0)
#define UNLOCK() ((void)0)
#endif
static LumeGame pending;
static uint32_t committed[18],desired[18];
static unsigned serial,done,written;
static bool initialized,ready,valid,queued,active,stopping,notified;
static void notify(void) {
 LOCK();bool running=ready;if(running)notified=true;UNLOCK();
#if defined(__3DS__)||defined(LUME_HOST_JOBS)
 if(running)SIGNAL();
#else
 (void)running;
#endif
}
#if defined(__3DS__)||defined(LUME_HOST_JOBS)
#ifdef __3DS__
static void perform(void *unused) {
#else
static void *perform(void *unused) {
#endif
 (void)unused;
 for(;;) {
#ifdef __3DS__
  LightEvent_Wait(&wake);LOCK();
#else
  LOCK();while(!queued&&!stopping&&!notified)pthread_cond_wait(&wake,&lock);
#endif
  notified=false;UNLOCK();lume_log_io_drain();LOCK();
  if(!queued){bool stop=stopping;UNLOCK();if(stop)break;continue;}
  LumeGame snapshot=pending;unsigned ticket=serial;queued=false;active=true;UNLOCK();
#ifdef __3DS__
  uint64_t started=osGetTime();
#endif
  bool ok=lume_game_save(&snapshot,LUME_DATA_FILE("save.lum"));
  LOCK();active=false;done=ticket;
  if(ok){lume_game_snapshot(&snapshot,committed);valid=true;written=ticket;}
  bool more=queued||stopping;UNLOCK();
#ifdef __3DS__
  lume_log("SAVE_IO ticket=%u ok=%d ms=%llu",ticket,ok,(unsigned long long)(osGetTime()-started));
#endif
  if(more)SIGNAL();
 }
#ifndef __3DS__
 return NULL;
#endif
}
#endif
bool lume_persist_start(void) {
 if(ready)return true;
 if(!initialized){
#ifdef __3DS__
  LightLock_Init(&lock);LightEvent_Init(&wake,RESET_ONESHOT);
#endif
  initialized=true;
 }
 stopping=false;notified=false;
#ifdef __3DS__
 /* Use the permitted application core at low priority. Do not compete with
    the shared Core 2 decoder worker or the music service's Core 1 budget. */
 worker=threadCreate(perform,NULL,32768,0x32,0,false);ready=worker!=NULL;
#elif defined(LUME_HOST_JOBS)
 ready=pthread_create(&worker,NULL,perform,NULL)==0;
#endif
 if(ready){lume_log_io_attach(notify);notify();}
 return ready;
}
void lume_persist_seed(const LumeGame *game) {
 if(!ready)return;
 LOCK();if(!active&&!queued){lume_game_snapshot(game,committed);memcpy(desired,committed,sizeof desired);valid=true;}UNLOCK();
}
unsigned lume_persist_request(const LumeGame *game) {
 if(!ready)return 0;
 uint32_t words[18];lume_game_snapshot(game,words);LOCK();
 if((active||queued)&&!memcmp(words,desired,sizeof words)) {unsigned ticket=serial;UNLOCK();return ticket;}
 if(!active&&!queued&&valid&&!memcmp(words,committed,sizeof words)) {
  if(!serial)serial=done=written=1;
  unsigned ticket=written;UNLOCK();return ticket;
 }
 pending=*game;memcpy(desired,words,sizeof words);queued=true;unsigned ticket=++serial;UNLOCK();notify();return ticket;
}
int lume_persist_status(unsigned ticket) {
 if(!ticket||!ready)return -1;
 LOCK();int result=written>=ticket?1:done>=ticket?-1:0;UNLOCK();return result;
}
void lume_persist_stop(void) {
 if(!ready)return;
 lume_log_io_attach(NULL);LOCK();stopping=true;UNLOCK();notify();
#ifdef __3DS__
 threadJoin(worker,U64_MAX);threadFree(worker);
#elif defined(LUME_HOST_JOBS)
 pthread_join(worker,NULL);
#endif
 LOCK();ready=false;UNLOCK();
 /* Locks/events have static lifetime. A producer that copied the old wake
    callback before detach can safely finish without touching freed state. */
 lume_log_io_drain();
}
