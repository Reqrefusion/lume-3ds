#include "engine/core/jobs_state.h"
#ifdef _MSC_VER
#include <intrin.h>
static volatile long owner,submitted,busy;
bool lume_worker_claim(void) {
 if(_InterlockedCompareExchange(&owner,1,0)!=0){_InterlockedExchangeAdd(&busy,1);return false;}
 _InterlockedExchangeAdd(&submitted,1);return true;
}
void lume_worker_release(void){_InterlockedExchange(&owner,0);}
void lume_worker_counts(unsigned *out_submitted,unsigned *out_busy) {
 *out_submitted=(unsigned)_InterlockedCompareExchange(&submitted,0,0);
 *out_busy=(unsigned)_InterlockedCompareExchange(&busy,0,0);
}
#else
static volatile unsigned owner,submitted,busy;
bool lume_worker_claim(void) {
 if(!__sync_bool_compare_and_swap(&owner,0,1)){__sync_fetch_and_add(&busy,1);return false;}
 __sync_fetch_and_add(&submitted,1);return true;
}
void lume_worker_release(void){__sync_lock_release(&owner);}
void lume_worker_counts(unsigned *out_submitted,unsigned *out_busy) {
 *out_submitted=__sync_fetch_and_add(&submitted,0);*out_busy=__sync_fetch_and_add(&busy,0);
}
#endif

#include <stdio.h>
#include <string.h>
static char flags_directory[160];
/* Application policy is supplied during startup; the reusable engine has no
   game save folder or mandatory on-disk switches of its own. */
void lume_runtime_set_flags_directory(const char *directory) {
 snprintf(flags_directory,sizeof flags_directory,"%s",directory?directory:"");
}
bool lume_runtime_flag(const char *name) {
 if(!flags_directory[0]||!name||strstr(name,"..")||strchr(name,'/')||strchr(name,':'))return false;
 char path[208];if(snprintf(path,sizeof path,"%s/%s",flags_directory,name)>=(int)sizeof path)return false;
 FILE *f=fopen(path,"rb");if(!f)return false;fclose(f);return true;
}

#ifndef LUME_ENGINE_ASSET_ROOT
#ifdef __3DS__
#define LUME_ENGINE_ASSET_ROOT "romfs:"
#else
#define LUME_ENGINE_ASSET_ROOT "."
#endif
#endif
static char asset_directory[160]=LUME_ENGINE_ASSET_ROOT;
void lume_runtime_set_asset_directory(const char *directory) {
 snprintf(asset_directory,sizeof asset_directory,"%s",directory&&*directory?directory:".");
}
bool lume_runtime_asset_path(char *out,size_t bytes,const char *relative) {
 if(!relative||!*relative||strstr(relative,"..")||strchr(relative,':')||relative[0]=='/'||strchr(relative,'\\'))return false;
 int n=snprintf(out,bytes,"%s/%s",asset_directory,relative);return n>=0&&(size_t)n<bytes;
}
