#ifndef LUME_PLATE_CACHE_H
#define LUME_PLATE_CACHE_H
#include <stdint.h>
#include "../../engine/assets/stereo_field.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define LUME_PLATE_SLOTS 12u
#define LUME_PLATE_BYTES (320u*240u*3u)
typedef struct {
 struct {uint8_t *pixels;unsigned id,used;bool valid,right;LumeStereoField field;} slots[LUME_PLATE_SLOTS];
 unsigned clock,hits,reads;
} LumePlateCache;
/* The plate ID, not the last decoder, owns this static camera's field. */
static inline const int8_t *lume_plate_cache_disparity(const LumePlateCache *c,unsigned id) {
 for(unsigned i=0;i<LUME_PLATE_SLOTS;i++)
  if(c->slots[i].valid&&c->slots[i].id==id&&c->slots[i].field.valid)
   return c->slots[i].field.map;
 return NULL;
}
static inline void lume_plate_cache_close(LumePlateCache *c) {
 for(unsigned i=0;i<LUME_PLATE_SLOTS;i++){free(c->slots[i].pixels);lume_field_close(&c->slots[i].field);}
 memset(c,0,sizeof *c);
}
/* Source dissolves alternate the same photographic plates. Bounded, lazily allocated LRU
   entries avoid reopening/reinflating them; each cached eye remains exact.
   The right eye is read lazily, including enabling stereo on a still plate. */
static inline bool lume_plate_cache_frame(LumePlateCache *c,unsigned id,bool stereo,
 uint8_t *left,uint8_t *right,bool (*read)(const char*,void*,size_t)) {
 unsigned slot=LUME_PLATE_SLOTS;bool hit=false;
 for(unsigned i=0;i<LUME_PLATE_SLOTS;i++)if(c->slots[i].valid&&c->slots[i].id==id){slot=i;hit=true;break;}
 if(slot==LUME_PLATE_SLOTS) {
  slot=0;
  for(unsigned i=0;i<LUME_PLATE_SLOTS;i++) {
   if(!c->slots[i].valid){slot=i;break;}
   if(c->slots[i].used<c->slots[slot].used)slot=i;
  }
 }
 uint8_t *pixels=c->slots[slot].pixels;
 if(!pixels){pixels=malloc(2*LUME_PLATE_BYTES);c->slots[slot].pixels=pixels;}
 char path[80];
 if(!hit) {
  c->slots[slot].valid=false;c->slots[slot].right=false;
  snprintf(path,sizeof path,"romfs:/cinema_bg%u.rgb",id);c->reads++;
  if(!read(path,pixels?pixels:left,LUME_PLATE_BYTES))return false;
  lume_field_load(&c->slots[slot].field,path,320,240);
  if(pixels){c->slots[slot].id=id;c->slots[slot].valid=true;}
 }else c->hits++;
 if(pixels)memcpy(left,pixels,LUME_PLATE_BYTES);
 if(stereo) {
  if(!hit||!c->slots[slot].right) {
   uint8_t *out=pixels?pixels+LUME_PLATE_BYTES:right;
   snprintf(path,sizeof path,"romfs:/cinema_bg%u_R.rgb",id);c->reads++;
   if(!read(path,out,LUME_PLATE_BYTES))memcpy(out,left,LUME_PLATE_BYTES);
   if(pixels)c->slots[slot].right=true;
  }
  if(pixels)memcpy(right,pixels+LUME_PLATE_BYTES,LUME_PLATE_BYTES);
  const LumeStereoField *field=&c->slots[slot].field;
  if(field->valid)lume_field_draw(field,left,right,right,lume_stereo_strength());
 }else memcpy(right,left,LUME_PLATE_BYTES);
 c->slots[slot].used=++c->clock;return true;
}
#endif
