#include "engine/animation/baked.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define LE_BAKED_LIMIT (512u*1024u)
_Static_assert(sizeof(LeBakedFrame)==24,"baked frame ABI");
_Static_assert(sizeof(LeBakedMove)==68,"baked move ABI");
static uint32_t read32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
uint32_t le_baked_fingerprint(const void *data,size_t size){const unsigned char *p=data;uint32_t hash=2166136261u;for(size_t i=0;i<size;i++)hash=(hash^p[i])*16777619u;return hash;}
void le_baked_free(LeBakedAnimation *a){free(a->bytes);memset(a,0,sizeof *a);}
bool le_baked_load(LeBakedAnimation *a,const char *path,const void *source,size_t source_size,unsigned slots){
 le_baked_free(a);FILE *f=fopen(path,"rb");if(!f)return false;
 unsigned char h[32];if(fread(h,1,sizeof h,f)!=sizeof h){fclose(f);return false;}
 unsigned nf=read32(h+16),nm=read32(h+20);
 bool valid=!memcmp(h,"LBK1",4)&&read32(h+4)==1&&read32(h+12)==source_size&&read32(h+28)==0&&nf<=16384&&nm<=16384;
 size_t size=(size_t)nf*24+(size_t)nm*68;
 if(!valid||size>LE_BAKED_LIMIT-32||read32(h+8)!=le_baked_fingerprint(source,source_size)){fclose(f);return false;}
 unsigned char *bytes=malloc(size?size:1);if(!bytes){fclose(f);return false;}
 valid=fread(bytes,1,size,f)==size&&fgetc(f)==EOF;fclose(f);
 valid=valid&&read32(h+24)==le_baked_fingerprint(bytes,size);
 /* Current targets are little-endian ARM11/x86; fail closed on other hosts. */
 const unsigned endian=1;if(*(const unsigned char*)&endian!=1)valid=false;
 LeBakedFrame *frames=(LeBakedFrame*)bytes;LeBakedMove *moves=(LeBakedMove*)(bytes+nf*24);
 for(unsigned i=0;valid&&i<nf;i++){
  const LeBakedFrame *p=frames+i;
  valid=p->pointer<p->next&&p->next<=source_size&&p->first<=nm&&p->count<=nm-p->first&&p->count<=128;
  if(i)valid=valid&&(p->clip>frames[i-1].clip||(p->clip==frames[i-1].clip&&p->frame>frames[i-1].frame));
 }
 for(unsigned i=0;valid&&i<nm;i++){
  const LeBakedMove *p=moves+i;valid=p->slot<slots&&!(p->mask&~248u)&&!((p->mask&64)&&(p->mask&128));
  for(unsigned j=0;j<15;j++)valid=valid&&isfinite(p->value[j]);
 }
 if(!valid){free(bytes);return false;}
 a->bytes=bytes;a->frames=frames;a->moves=moves;a->frame_count=nf;a->move_count=nm;return true;
}
