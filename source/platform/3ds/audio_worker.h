#ifndef LUME_WORKER_H
#define LUME_WORKER_H
#include <3ds.h>
static inline Thread lume_music_worker(ThreadFunc entry,int *core) {
 Thread worker=NULL;*core=-1;
 if(R_SUCCEEDED(APT_SetAppCpuTimeLimit(30))) {
  worker=threadCreate(entry,NULL,64*1024,0x30,1,false);
  if(worker){*core=1;return worker;}
 }
 bool newer=false;
 if(R_SUCCEEDED(APT_CheckNew3DS(&newer))&&newer) {
  worker=threadCreate(entry,NULL,64*1024,0x30,2,false);
  if(worker)*core=2;
 }
 return worker;
}
#endif
