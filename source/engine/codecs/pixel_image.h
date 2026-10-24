#ifndef LUME_PIXEL_IMAGE_H
#define LUME_PIXEL_IMAGE_H
#include "engine/codecs/pixel_predict.h"
#include <stdio.h>
#include <string.h>
#include <zlib.h>
#include <lz4.h>
#include <stdlib.h>
#include "engine/core/jobs_state.h"
/* Page paths retain their original names. A tagged lossless payload replaces
   raw RGB only when smaller; generated raw pages remain valid inputs. */
static inline bool lume_pixel_image_depth(FILE *f,uint8_t *out,unsigned w,unsigned h,unsigned depth) {
 size_t bytes=(size_t)w*h*3;
 if(fseek(f,0,SEEK_END))return false;
 long end=ftell(f);
 if(end<0||fseek(f,0,SEEK_SET))return false;
 if((unsigned long)end==bytes)
  return fread(out,1,bytes,f)==bytes&&!ferror(f);
 uint8_t tag[4];
 if(end>=6&&fread(tag,1,4,f)==4&&!memcmp(tag,"LRF1",4)) {
  uint8_t length[2];if(depth||fread(length,1,2,f)!=2)return false;
  unsigned n=length[0]|length[1]<<8;
  if(n<8||n>100||end!=(long)(n+6))return false;
  char relative[128],ref[300];
  if(fread(relative,1,n,f)!=n)return false;
  relative[n]=0;
  if(memcmp(relative,"detail/",7)||strstr(relative,".."))return false;
  for(unsigned i=0;i<n;i++)if(!((relative[i]>='a'&&relative[i]<='z')||(relative[i]>='A'&&relative[i]<='Z')||(relative[i]>='0'&&relative[i]<='9')||relative[i]=='_'||relative[i]=='/'||relative[i]=='.'))return false;
  if(!lume_runtime_asset_path(ref,sizeof ref,relative))return false;
  FILE *page=fopen(ref,"rb");if(!page)return false;
  bool ok=lume_pixel_image_depth(page,out,w,h,depth+1);
  if(fclose(page))ok=false;
  return ok;
 }
 if(fseek(f,0,SEEK_SET))return false;
 uint8_t head[13];
 if(end<=13||end>4*1024*1024||fread(head,1,13,f)!=13)return false;
 if(memcmp(head,"DZP1",4)&&memcmp(head,"DZP2",4)&&memcmp(head,"DZP3",4))return false;
 unsigned header_size=head[3]=='1'?12:13;
 unsigned filter=header_size==12?0:head[12];
 if(header_size==12 && fseek(f,12,SEEK_SET))return false;
 uint32_t iw=0,ih=0;
 for(unsigned i=0;i<4;i++){iw|=(uint32_t)head[4+i]<<(8*i);ih|=(uint32_t)head[8+i]<<(8*i);}
 if(iw!=w||ih!=h||filter>2)return false;
 if(!memcmp(head,"DZP3",4)) {
  size_t n=(size_t)end-13;uint8_t *packet=malloc(n);
  bool ok=packet&&fread(packet,1,n,f)==n&&LZ4_decompress_safe((const char*)packet,(char*)out,(int)n,(int)bytes)==(int)bytes;
  free(packet);return ok&&lume_pixel_restore(out,w,h,3,filter);
 }
 z_stream z={0};if(inflateInit(&z)!=Z_OK)return false;
 z.next_out=out;z.avail_out=bytes;
 uint8_t chunk[4096];size_t left=(size_t)end-header_size;int rc=Z_OK;
 while(left&&rc==Z_OK) {
  size_t n=left<sizeof chunk?left:sizeof chunk;
  if(fread(chunk,1,n,f)!=n)break;
  left-=n;z.next_in=chunk;z.avail_in=n;rc=inflate(&z,Z_NO_FLUSH);
 }
 bool ok=rc==Z_STREAM_END&&!left&&!z.avail_in&&z.total_out==bytes;
 inflateEnd(&z);
 return ok&&lume_pixel_restore(out,w,h,3,filter);
}
static inline bool lume_pixel_image(FILE *f,uint8_t *out,unsigned w,unsigned h) {
 return lume_pixel_image_depth(f,out,w,h,0);
}
#endif
