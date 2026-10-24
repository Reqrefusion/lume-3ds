#include "engine/core/log_io.h"
#include <stdio.h>
#include <string.h>
#ifdef __3DS__
#include <3ds.h>
static LightLock lock;
#define LOCK() LightLock_Lock(&lock)
#define UNLOCK() LightLock_Unlock(&lock)
#elif defined(LUME_HOST_JOBS)
#include <pthread.h>
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
#define LOCK() pthread_mutex_lock(&lock)
#define UNLOCK() pthread_mutex_unlock(&lock)
#else
#define LOCK() ((void)0)
#define UNLOCK() ((void)0)
#endif
#define CAPACITY 32768
static char ring[CAPACITY],log_path[256];
static unsigned head,used,dropped;
static void (*notify)(void);
/* Called once before any producer threads start. */
void lume_log_io_init(const char *path) {
#ifdef __3DS__
 LightLock_Init(&lock);
#endif
 head=used=dropped=0;notify=NULL;snprintf(log_path,sizeof log_path,"%s",path?path:"");
}
void lume_log_io_attach(void (*wake)(void)) {LOCK();notify=wake;UNLOCK();}
void lume_log_io_writev(const char *format,va_list args) {
 char line[1024];int n=vsnprintf(line,sizeof line-1,format,args);
 if(n<0)return;
 unsigned len=(unsigned)n;if(len>sizeof line-2)len=sizeof line-2;line[len++]='\n';
 LOCK();
 if(len>CAPACITY-used)dropped++;
 else {for(unsigned i=0;i<len;i++)ring[(head+used+i)%CAPACITY]=line[i];used+=len;}
 void (*wake)(void)=notify;UNLOCK();if(wake)wake();
}
void lume_log_io_drain(void) {
 /* Only one caller drains at a time; this never runs on a producer thread. */
 char chunk[4096];FILE *out=NULL;unsigned budget=CAPACITY;
 while(budget) {
  LOCK();unsigned n=0;
  while(n<used&&n<sizeof chunk&&n<budget){chunk[n]=ring[(head+n)%CAPACITY];n++;}
  if(n<used)while(n&&chunk[n-1]!='\n')n--;
  head=(head+n)%CAPACITY;used-=n;budget-=n;unsigned lost=dropped;dropped=0;UNLOCK();
  if(!n&&!lost)break;
  if(!out&&log_path[0])out=fopen(log_path,"a");
  if(out){if(lost)fprintf(out,"LOG dropped=%u\n",lost);if(n)fwrite(chunk,1,n,out);}
  if(!n)break;
 }
 if(out)fclose(out);
}
