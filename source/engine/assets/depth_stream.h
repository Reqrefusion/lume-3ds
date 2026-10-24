#ifndef LUME_DEPTH_STREAM_H
#define LUME_DEPTH_STREAM_H
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lz4.h>
/* Indexed, independent 160x120 signed quarter-pixel fields. No temporal
   replay, model inference or colour decoding takes place here. */
typedef struct {FILE *file;uint32_t *offsets;unsigned frames,last;int8_t *small,*map;} LumeDepthStream;
static inline uint32_t lume_depth_u32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static inline void lume_depth_close(LumeDepthStream *s){
 if(s->file)fclose(s->file);
 free(s->offsets);free(s->small);free(s->map);memset(s,0,sizeof *s);
}
static inline bool lume_depth_open(LumeDepthStream *s,const char *path,unsigned frames){
 lume_depth_close(s);s->file=fopen(path,"rb");if(!s->file)return false;
 uint8_t h[16],word[4];bool ok=!fseek(s->file,0,SEEK_END);long end=ok?ftell(s->file):-1;
 ok=ok&&end>20&&end<32*1024*1024&&!fseek(s->file,0,SEEK_SET)&&fread(h,1,16,s->file)==16;
 if(!ok||memcmp(h,"TSF1",4)||lume_depth_u32(h+4)!=160||lume_depth_u32(h+8)!=120||!frames||frames>10000||lume_depth_u32(h+12)!=frames)goto fail;
 s->offsets=malloc((frames+1)*sizeof(uint32_t));s->small=malloc(19200);s->map=malloc(76800);
 if(!s->offsets||!s->small||!s->map)goto fail;
 for(unsigned i=0;i<=frames;i++){
  if(fread(word,1,4,s->file)!=4)goto fail;
  s->offsets[i]=lume_depth_u32(word);
  if(i&&(s->offsets[i]<=s->offsets[i-1]||s->offsets[i]-s->offsets[i-1]>20000))goto fail;
 }
 if(s->offsets[0]!=16+4*(frames+1)||s->offsets[frames]!=(uint32_t)end)goto fail;
 s->frames=frames;s->last=~0u;return true;
fail:lume_depth_close(s);return false;
}
static inline void lume_depth_expand(const int8_t *small,int8_t *map){
 for(unsigned y=0;y<240;y++)for(unsigned x=0;x<320;x++){
  unsigned a=x/2,b=y/2,ax=a<159?a+1:a,by=b<119?b+1:b;
  int v=small[b*160+a]*(2-(x&1))*(2-(y&1))+small[b*160+ax]*(x&1)*(2-(y&1))+
        small[by*160+a]*(2-(x&1))*(y&1)+small[by*160+ax]*(x&1)*(y&1);
  map[y*320+x]=(int8_t)(v>=0?(v+2)/4:-((-v+2)/4));
 }
}
static inline const int8_t *lume_depth_frame(LumeDepthStream *s,unsigned frame){
 if(!s->file||frame>=s->frames)return NULL;
 if(s->last==frame)return s->map;
 unsigned n=s->offsets[frame+1]-s->offsets[frame];char *packet=malloc(n);if(!packet)return NULL;
 bool ok=!fseek(s->file,s->offsets[frame],SEEK_SET)&&fread(packet,1,n,s->file)==n&&LZ4_decompress_safe(packet,(char*)s->small,n,19200)==19200;
 free(packet);if(!ok){s->last=~0u;return NULL;}
 lume_depth_expand(s->small,s->map);s->last=frame;return s->map;
}
#endif
