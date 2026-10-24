#ifndef LUME_ASSET_IO_H
#define LUME_ASSET_IO_H
#include "engine/codecs/pixel_predict.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lz4.h>
/* Static raster compression is decoded only at asset load, never once per
   video frame. Both eyes retain their own exact original RGB bytes. */
static inline bool lume_asset_read(FILE *f,void *out,size_t bytes) {
 if(fseek(f,0,SEEK_END))return false;
 long end=ftell(f);
 if(end<0||fseek(f,0,SEEK_SET))return false;
 if((unsigned long)end==bytes)return fread(out,1,bytes,f)==bytes&&!ferror(f);
 uint8_t h[16];
 if(end<=16||end>4*1024*1024||fread(h,1,16,f)!=16||memcmp(h,"LRC1",4))return false;
 uint32_t n=(uint32_t)h[4]|(uint32_t)h[5]<<8|(uint32_t)h[6]<<16|(uint32_t)h[7]<<24;
 unsigned w=h[8]|h[9]<<8,ht=h[10]|h[11]<<8,c=h[12],mode=h[13];
 if(n!=bytes||!w||!ht||(c!=3&&c!=4)||(uint64_t)w*ht*c!=bytes||mode>2||h[14]!=1||h[15])return false;
 size_t size=(size_t)end-16;uint8_t *packet=malloc(size);if(!packet)return false;
 bool ok=fread(packet,1,size,f)==size&&!ferror(f)&&
   LZ4_decompress_safe((const char*)packet,(char*)out,(int)size,(int)bytes)==(int)bytes;
 free(packet);
 return ok&&lume_pixel_restore(out,w,ht,c,mode);
}
#endif
