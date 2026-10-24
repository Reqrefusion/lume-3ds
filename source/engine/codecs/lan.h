#ifndef LUME_LAN_READER_H
#define LUME_LAN_READER_H
#include "engine/assets/file_cache.h"
#include "engine/codecs/pixel_predict.h"
#include <lz4.h>
#include <zlib.h>
#include <stdlib.h>
#include <string.h>
#define LUME_LAN_REF (1u<<27)
#define LUME_LAN_DELTA (1u<<26)
#define LUME_LAN_GROUP 8u
typedef struct {
 FILE *file;LumeFileCache *cache;const uint32_t *offsets;
 unsigned frames,width,height;uint32_t total;
 bool mixed,spatial,temporal;
} LumeLanInput;
typedef struct {
 unsigned prepared,width,height,pixel_anchor;
 uint8_t *residual,*compressed;size_t capacity,compressed_capacity;
 z_stream inflate;bool inflate_ready;
 unsigned lz4_blocks,zlib_blocks,reused_blocks,delta_blocks;
} LumeLanState;
typedef struct {
 unsigned tag,background,width,height,body,size;
 int x,y;
} LumeLanRecord;
static inline uint32_t lume_lan_le(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static inline void lume_lan_close(LumeLanState *s){if(s->inflate_ready)inflateEnd(&s->inflate);free(s->residual);free(s->compressed);memset(s,0,sizeof *s);s->prepared=~0u;}
static inline bool lume_lan_read(const LumeLanInput *in,uint32_t offset,void *out,unsigned bytes){
 if(offset>in->total || bytes>in->total-offset)return false;
 if(in->cache){const uint8_t *p=lume_cache_get(in->cache,offset,bytes);if(!p)return false;memcpy(out,p,bytes);return true;}
 return fseek(in->file,(long)offset,SEEK_SET)==0 && fread(out,1,bytes,in->file)==bytes;
}
static inline bool lume_lan_record(const LumeLanInput *in,unsigned frame,LumeLanRecord *r){
 if(frame>=in->frames)return false;
 uint32_t a,b;uint8_t ix[8],p[12];
 if(in->offsets){a=in->offsets[frame];b=in->offsets[frame+1];}
 else {if(!lume_lan_read(in,20+frame*4,ix,8))return false;a=lume_lan_le(ix);b=lume_lan_le(ix+4);}
 if(a<20+4*(in->frames+1) || b<a || b-a<12 || b-a>4*1024*1024 || b>in->total || !lume_lan_read(in,a,p,12))return false;
 r->tag=lume_lan_le(p);r->x=(int16_t)(p[4]|(unsigned)p[5]<<8);r->y=(int16_t)(p[6]|(unsigned)p[7]<<8);
 r->width=p[8]|(unsigned)p[9]<<8;r->height=p[10]|(unsigned)p[11]<<8;r->body=a+12;r->size=b-a-12;
 r->background=r->tag&(in->temporal?0x03ffffffu:in->spatial?0x0fffffffu:in->mixed?0x7fffffffu:~0u);
 return r->x>=0 && r->y>=0 && r->width<=in->width && r->height<=in->height &&
  (unsigned)r->x+r->width<=in->width && (unsigned)r->y+r->height<=in->height &&
  !!r->width==!!r->height;
}
static inline bool lume_lan_reference(const LumeLanInput *in,unsigned frame,LumeLanRecord *r,unsigned *pixel_anchor){
 *pixel_anchor=frame;
 if(!in->temporal || !(r->tag&LUME_LAN_REF))return true;
 uint8_t ref[4];LumeLanRecord anchor;
 if(r->size!=4 || (r->tag&(LUME_LAN_DELTA|0xf0000000u)) || !lume_lan_read(in,r->body,ref,4))return false;
 unsigned target=lume_lan_le(ref);
 if(target>=frame || target/LUME_LAN_GROUP!=frame/LUME_LAN_GROUP || !lume_lan_record(in,target,&anchor) || (anchor.tag&(LUME_LAN_REF|LUME_LAN_DELTA)) ||
    anchor.width!=r->width || anchor.height!=r->height)return false;
 /* Coordinates and camera IDs belong to this frame, only pixels are shared. */
 r->body=anchor.body;r->size=anchor.size;r->tag=anchor.tag;*pixel_anchor=target;return true;
}
static inline bool lume_lan_decode(const LumeLanInput *in,LumeLanState *s,unsigned frame,
                                   uint8_t *out,size_t capacity,LumeLanRecord *result){
 LumeLanRecord wanted,r;
 if(!lume_lan_record(in,frame,&wanted))goto failed;
 unsigned first=frame;
 if(in->temporal && (wanted.tag&LUME_LAN_DELTA)){
  if(!(frame%LUME_LAN_GROUP))goto failed;
  if(s->prepared!=frame-1){
   /* A clocked clip can skip displayed poses without invalidating its last
      exact decoded pixels. Continue forward inside the same anchor group;
      replaying the anchor would solve already available frames twice. */
   if(s->prepared!=~0u && s->prepared<frame &&
      s->prepared/LUME_LAN_GROUP==frame/LUME_LAN_GROUP)first=s->prepared+1;
   else {
    r=wanted;
    while(r.tag&LUME_LAN_DELTA){
     if(!first || first%LUME_LAN_GROUP==0 || !lume_lan_record(in,--first,&r))goto failed;
    }
   }
  }
 }
 for(unsigned i=first;i<=frame;i++){
  if(!lume_lan_record(in,i,&r))goto failed;
  bool delta=in->temporal && (r.tag&LUME_LAN_DELTA);
  if(delta && (!(i%LUME_LAN_GROUP) || s->prepared!=i-1 || s->width!=r.width || s->height!=r.height))goto failed;
  unsigned pixel_anchor;
  if(!lume_lan_reference(in,i,&r,&pixel_anchor))goto failed;
  unsigned filter=in->spatial?(r.tag>>28)&7u:0;
  bool fast=in->mixed && (r.tag&0x80000000u);
  size_t bytes=(size_t)r.width*r.height*4;
  if(filter>2 || bytes>capacity || (bytes&&!r.size) || (!bytes && (r.size || delta)))goto failed;
  bool reused=!delta && s->prepared!=~0u && s->pixel_anchor==pixel_anchor &&
    s->width==r.width && s->height==r.height;
  if(bytes && !reused){
   uint8_t *dst=out;
   if(delta){
    if(s->capacity<bytes){uint8_t *p=realloc(s->residual,bytes);if(!p)goto failed;s->residual=p;s->capacity=bytes;}
    dst=s->residual;
   }
   const uint8_t *body;
   if(in->cache)body=lume_cache_get(in->cache,r.body,r.size);
   else {
    if(s->compressed_capacity<r.size){uint8_t *p=realloc(s->compressed,r.size);if(!p)goto failed;s->compressed=p;s->compressed_capacity=r.size;}
    body=s->compressed;if(!lume_lan_read(in,r.body,s->compressed,r.size))goto failed;
   }
   if(!body)goto failed;
   bool ok;
   if(fast)ok=LZ4_decompress_safe((const char*)body,(char*)dst,(int)r.size,(int)bytes)==(int)bytes;
   else {
    if(!s->inflate_ready)s->inflate_ready=inflateInit(&s->inflate)==Z_OK;
    ok=s->inflate_ready && inflateReset(&s->inflate)==Z_OK;
    if(ok){s->inflate.next_in=(Bytef*)body;s->inflate.avail_in=r.size;s->inflate.next_out=dst;s->inflate.avail_out=(uInt)bytes;
     ok=inflate(&s->inflate,Z_FINISH)==Z_STREAM_END && s->inflate.total_out==bytes && !s->inflate.avail_in;}
   }
   if(!ok || !lume_pixel_restore(dst,r.width,r.height,4,filter))goto failed;
   if(fast)s->lz4_blocks++;else s->zlib_blocks++;
   if(delta)s->delta_blocks++;
   if(delta)for(size_t j=0;j<bytes;j++)out[j]=(uint8_t)(out[j]+dst[j]);
  }
  if(bytes&&reused)s->reused_blocks++;
  s->prepared=i;s->width=r.width;s->height=r.height;s->pixel_anchor=delta?~0u:pixel_anchor;
 }
 *result=wanted;return true;
failed:
 s->prepared=~0u;return false;
}
#endif
