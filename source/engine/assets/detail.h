#ifndef LUME_DETAIL_H
#define LUME_DETAIL_H
#include "engine/render/blit.h"
#include "engine/codecs/pixel_predict.h"
#include "engine/codecs/lan.h"
#include "engine/render/viewport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "engine/core/jobs_state.h"
/* The bottom camera reads original-sized assets on demand. One page and one
   reusable crop bound memory independently of the number of source frames. */
#define LUME_DETAIL_W 800u
#define LUME_DETAIL_HGT 600u
#define LUME_DETAIL_BYTES (800u*600u*3u)
typedef struct {
 uint8_t *rgb,*raw,*base;size_t capacity;
 char base_asset[80];
 FILE *layer_file;uint32_t *layer_offsets;unsigned layer_frames,layer_total;
 bool layer_temporal,layer_spatial,layer_mixed;char open_layer[80];
 LumeLanState lan_state;
 unsigned key;char asset[80];bool valid;
 unsigned loads,layers,failures;
 char layer_asset[80];unsigned layer_frame,lw,lh;int lx,ly;bool layer_valid;
} LumeDetail;
static inline uint32_t lume_detail_le(const uint8_t *p) {
 return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static inline void lume_detail_close(LumeDetail *d) {
 lume_lan_close(&d->lan_state);
 if(d->layer_file)fclose(d->layer_file);
 free(d->layer_offsets);free(d->base);free(d->rgb);free(d->raw);memset(d,0,sizeof *d);
}
static inline bool lume_detail_raw(LumeDetail *d,size_t size) {
 if(size>800u*600u*4u)return false;
 if(size<=d->capacity)return true;
 uint8_t *p=realloc(d->raw,size);if(!p)return false;
 d->raw=p;d->capacity=size;return true;
}
static inline bool lume_detail_inflate(FILE *f,unsigned bytes,uint8_t *out,unsigned size) {
 z_stream z={0};if(inflateInit(&z)!=Z_OK)return false;
 uint8_t chunk[4096];z.next_out=out;z.avail_out=size;int rc=Z_OK;
 while(bytes && rc==Z_OK) {
  unsigned n=bytes<sizeof chunk?bytes:sizeof chunk;
  if(fread(chunk,1,n,f)!=n)break;
  bytes-=n;z.next_in=chunk;z.avail_in=n;
  rc=inflate(&z,Z_NO_FLUSH);
  if(z.avail_in && rc!=Z_STREAM_END)break;
 }
 bool ok=rc==Z_STREAM_END&&!bytes&&!z.avail_in&&z.total_out==size;
 inflateEnd(&z);return ok;
}
static inline int lume_detail_select(LumeDetail *d,const char *asset,unsigned key) {
 if(!strcmp(d->asset,asset)&&d->key==key)return d->valid?0:-1;
 snprintf(d->asset,sizeof d->asset,"%s",asset);d->key=key;d->valid=false;
 if(!d->rgb)d->rgb=malloc(LUME_DETAIL_BYTES);
 return d->rgb?1:-1;
}
/* Returns 1 for a newly loaded page, 0 for an exact cache hit, -1 on failure.
   A missing/corrupt page always falls back to current gameplay, never an old
   page. Changed state keys retry without repeatedly reopening a failed file. */
static inline int lume_detail_begin(LumeDetail *d,const char *name,unsigned key) {
 int changed=lume_detail_select(d,name,key);if(changed!=1)return changed;
 char path[256],relative[112];snprintf(relative,sizeof relative,"detail/%s.dzp",name);
 if(!lume_runtime_asset_path(path,sizeof path,relative))return -1;
 FILE *f=fopen(path,"rb");if(!f){d->failures++;return -1;}
 uint8_t h[12];bool ok=fseek(f,0,SEEK_END)==0;long end=ok?ftell(f):-1;
 ok=end>12&&end<=4*1024*1024&&fseek(f,0,SEEK_SET)==0&&fread(h,1,12,f)==12&&
    (!memcmp(h,"DZP1",4)||!memcmp(h,"DZP2",4)||!memcmp(h,"DZP3",4))&&lume_detail_le(h+4)==800&&lume_detail_le(h+8)==600;
 unsigned filter=0,extra=0;
 if(ok&&memcmp(h,"DZP1",4)){int c=fgetc(f);ok=c>=0&&c<=2;filter=(unsigned)c;extra=1;}
 if(ok&&!memcmp(h,"DZP3",4)) {
  unsigned bytes=(unsigned)end-13;
  uint8_t *packet=malloc(bytes);
  ok=packet&&fread(packet,1,bytes,f)==bytes&&
   LZ4_decompress_safe((const char*)packet,(char*)d->rgb,(int)bytes,LUME_DETAIL_BYTES)==(int)LUME_DETAIL_BYTES;
  free(packet);
 }else if(ok)ok=lume_detail_inflate(f,(unsigned)end-12-extra,d->rgb,LUME_DETAIL_BYTES);
 if(ok)ok=lume_pixel_restore(d->rgb,800,600,3,filter);
 if(fclose(f))ok=false;
 d->valid=ok;if(ok)d->loads++;else d->failures++;
 return ok?1:-1;
}
/* State changes restore a clean photographed board from RAM. The single
   baseline is allocated only for puzzles, not every actor/paper cache. */
static inline int lume_detail_board_begin(LumeDetail *d,const char *name,unsigned key) {
 if(!strcmp(d->asset,name)&&d->key==key)return d->valid?0:-1;
 if(!d->base || strcmp(d->base_asset,name)) {
  if(lume_detail_begin(d,name,0)<0)return -1;
  if(!d->base)d->base=malloc(LUME_DETAIL_BYTES);
  if(!d->base)return lume_detail_begin(d,name,key);
  memcpy(d->base,d->rgb,LUME_DETAIL_BYTES);
  snprintf(d->base_asset,sizeof d->base_asset,"%s",name);
 }
 if(lume_detail_select(d,name,key)<0)return -1;
 memcpy(d->rgb,d->base,LUME_DETAIL_BYTES);d->valid=true;return 1;
}
/* Retain one checked layer index and FILE handle. A solar board contains 64
   tiles from the same stream; reopening it for every tile stalls touch input. */
static inline bool lume_detail_layer_open(LumeDetail *d,const char *name) {
 if(d->layer_file&&!strcmp(d->open_layer,name))return true;
 if(d->layer_file)fclose(d->layer_file);
 d->layer_file=NULL;free(d->layer_offsets);d->layer_offsets=NULL;
 d->layer_valid=false;d->lan_state.prepared=~0u;
 char path[256],relative[112];snprintf(relative,sizeof relative,"detail/%s.lan",name);
 if(!lume_runtime_asset_path(path,sizeof path,relative))return false;
 FILE *f=fopen(path,"rb");if(!f)return false;
 uint8_t h[20];bool ok=fread(h,1,20,f)==20 &&
  (!memcmp(h,"LAN2",4)||!memcmp(h,"LAN3",4)||!memcmp(h,"LAN4",4)||!memcmp(h,"LAN5",4)) &&
  lume_detail_le(h+4)==800 && lume_detail_le(h+8)==600;
 unsigned n=ok?lume_detail_le(h+16):0;
 ok=ok && n>0 && n<=10000;
 uint32_t *index=ok?malloc((n+1)*sizeof *index):NULL;ok=ok&&index;
 if(ok)ok=fread(index,4,n+1,f)==n+1;
 long end=-1;if(ok&&fseek(f,0,SEEK_END)==0)end=ftell(f);
 ok=ok&&end>=0&&(unsigned long)end<=UINT32_MAX;
 if(ok)for(unsigned i=0;i<=n;i++) {
  index[i]=lume_detail_le((const uint8_t*)(index+i));
  if((!i&&index[i]!=20+4*(n+1)) || (i&&index[i]<=index[i-1]) || index[i]>(unsigned long)end)ok=false;
 }
 if(ok)ok=index[n]==(uint32_t)end;
 if(!ok){free(index);fclose(f);return false;}
 d->layer_file=f;d->layer_offsets=index;d->layer_frames=n;d->layer_total=(uint32_t)end;
 d->layer_temporal=!memcmp(h,"LAN5",4);d->layer_spatial=!memcmp(h,"LAN4",4)||d->layer_temporal;
 d->layer_mixed=memcmp(h,"LAN2",4)!=0;
 snprintf(d->open_layer,sizeof d->open_layer,"%s",name);return true;
}
static inline bool lume_detail_load_layer(LumeDetail *d,const char *name,unsigned frame,int *ox,int *oy,unsigned *ow,unsigned *oh) {
 if(d->layer_valid&&d->layer_frame==frame&&!strcmp(d->layer_asset,name)) {
  *ox=d->lx;*oy=d->ly;*ow=d->lw;*oh=d->lh;return true;
 }
 bool forward=d->layer_valid&&!strcmp(d->layer_asset,name)&&
  frame>d->layer_frame&&frame/LUME_LAN_GROUP==d->layer_frame/LUME_LAN_GROUP;
 d->layer_valid=false;if(!forward)d->lan_state.prepared=~0u;
 if(!lume_detail_layer_open(d,name) || frame>=d->layer_frames)return false;
 bool ok=true;
 LumeLanInput input={.file=d->layer_file,.offsets=d->layer_offsets,
  .frames=d->layer_frames,.width=800,.height=600,.total=d->layer_total,
  .temporal=d->layer_temporal,.spatial=d->layer_spatial,.mixed=d->layer_mixed};
 LumeLanRecord record={0};
 if(ok)ok=lume_lan_record(&input,frame,&record);
 /* Earlier crops within a temporal group have the same dimensions as the
    requested crop; a larger allocation is needed only for a new anchor. */
 if(ok)ok=lume_detail_raw(d,(size_t)record.width*record.height*4) &&
  lume_lan_decode(&input,&d->lan_state,frame,d->raw,d->capacity,&record);
 int x=record.x,y=record.y;unsigned w=record.width,ht=record.height;
 if(!ok){d->lan_state.prepared=~0u;d->failures++;return false;}
 *ox=x;*oy=y;*ow=w;*oh=ht;d->layers++;
 d->lx=x;d->ly=y;d->lw=w;d->lh=ht;d->layer_frame=frame;d->layer_valid=true;
 snprintf(d->layer_asset,sizeof d->layer_asset,"%s",name);return true;
}
static inline bool lume_detail_layer_shift(LumeDetail *d,const char *name,unsigned frame,int dx,int dy) {
 int x,y;unsigned w,ht;
 if(!d->valid||!lume_detail_load_layer(d,name,frame,&x,&y,&w,&ht))return false;
 for(unsigned yy=0;yy<ht;yy++)for(unsigned xx=0;xx<w;xx++) {
  int tx=x+(int)xx+dx,ty=y+(int)yy+dy;
  if(tx<0||tx>=800||ty<0||ty>=600)continue;
  const uint8_t *q=d->raw+(yy*w+xx)*4;uint8_t *dst=d->rgb+(ty*800+tx)*3;
  if(q[3])lume_blend_pixel(dst,q,q[3]);
 }
 return true;
}
static inline bool lume_detail_layer(LumeDetail *d,const char *name,unsigned frame) {
 return lume_detail_layer_shift(d,name,frame,0,0);
}
/* Foreground source crops are projected directly into the bottom camera.
   They never alter the cached clean page or the stereo overview. */
static inline void lume_detail_overlay_rows(const LumeDetail *d,const LumeViewport *v,
    uint8_t *out,int ox,int oy,unsigned w,unsigned h,unsigned first,unsigned last) {
 bool filter=v->scale<2560||lume_view_moving(v);
 int samples[320],fractions[320];
 for(int x=0;x<320;x++) {
  int64_t px=((int64_t)v->cx*64+(int64_t)(2*x-319)*32768*1024/v->scale)*800/320;
  int qx=(int)(px-(int64_t)ox*65536-(filter?32768:0));
  samples[x]=qx>=0?qx/65536:-1-(int)((-(int64_t)qx-1)/65536);
  fractions[x]=((unsigned)qx>>8)&255;
 }
 for(int y=(int)first;y<(int)last;y++) {
  int64_t py=((int64_t)v->cy*64+(int64_t)(2*y-239)*32768*1024/v->scale)*600/240;
  int qy=(int)(py-(int64_t)oy*65536-(filter?32768:0));
  int sy=qy>=0?qy/65536:-1-(int)((-(int64_t)qy-1)/65536);
  if(sy<(filter?-1:0)||sy>=(int)h)continue;
  unsigned fy=((unsigned)qy>>8)&255;
  for(int x=0;x<320;x++) {
   int sx=samples[x];
   if(sx<(filter?-1:0)||sx>=(int)w)continue;
   uint8_t *dst=out+(y*320+x)*3;
   if(!filter) {
    const uint8_t *q=d->raw+((unsigned)sy*w+(unsigned)sx)*4;
    if(q[3])lume_blend_pixel(dst,q,q[3]);
   } else {
    /* Filter premultiplied alpha when reducing original glyphs; transparent
       black outside a crop must not add dark fringes to its paper/actor. */
    unsigned fx=(unsigned)fractions[x],a=0,r=0,g=0,b=0;
    for(unsigned dy=0;dy<2;dy++)for(unsigned dx=0;dx<2;dx++) {
     int xx=sx+(int)dx,yy=sy+(int)dy;
     if(xx<0||yy<0||(unsigned)xx>=w||(unsigned)yy>=h)continue;
     const uint8_t *q=d->raw+((unsigned)yy*w+(unsigned)xx)*4;
     unsigned weight=(dx?fx:256-fx)*(dy?fy:256-fy)*q[3];
     a+=weight;r+=weight*q[0];g+=weight*q[1];b+=weight*q[2];
    }
    const unsigned den=65536u*255u;
    dst[0]=(r+(den-a)*dst[0]+den/2)/den;
    dst[1]=(g+(den-a)*dst[1]+den/2)/den;
    dst[2]=(b+(den-a)*dst[2]+den/2)/den;
   }
  }
 }
}
typedef struct {const LumeDetail *detail;const LumeViewport *view;uint8_t *out;int x,y;unsigned w,h;} LumeDetailViewJob;
static inline void lume_detail_view_job(void *arg) {
 LumeDetailViewJob *j=arg;lume_detail_overlay_rows(j->detail,j->view,j->out,j->x,j->y,j->w,j->h,120,240);
}
static inline void lume_detail_overlay_view(const LumeDetail *d,const LumeViewport *v,uint8_t *out,int x,int y,unsigned w,unsigned h) {
 lume_detail_overlay_rows(d,v,out,x,y,w,h,0,240);
}
static inline void lume_detail_overlay_parallel(const LumeDetail *d,const LumeViewport *v,uint8_t *out,int x,int y,unsigned w,unsigned h,LumeMovieJobs *jobs) {
 LumeDetailViewJob j={d,v,out,x,y,w,h};
 if(lume_jobs_start(jobs,lume_detail_view_job,&j)) {
  lume_detail_overlay_rows(d,v,out,x,y,w,h,0,120);lume_jobs_wait(jobs);
 }else lume_detail_overlay_view(d,v,out,x,y,w,h);
}
#endif
