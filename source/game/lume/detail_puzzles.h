#ifndef LUME_DETAIL_PUZZLES_H
#define LUME_DETAIL_PUZZLES_H
#include "engine/assets/detail.h"
#include "puzzles.h"
#include <math.h>
static inline unsigned lume_detail_hash(unsigned h,unsigned v){return (h^v)*16777619u;}
static inline bool lume_detail_wire_preview(LumeDetail *d,const WirePuzzle *p) {
 static const float points[12][2]={{649.7f,328.45f},{649.7f,425.1f},{306.45f,324.15f},{311.4f,437.1f},{657.4f,108.75f},{433.55f,109.65f},{308.75f,108.75f},{543.3f,109.65f},{307.35f,208.25f},{535.3f,430.1f},{648.1f,204.7f},{429.25f,433.1f}};
 if(p->selected<0||p->selected>=12||p->preview_x<0||p->preview_x>=320||p->preview_y<13||p->preview_y>=227)return true;
 if(!lume_detail_raw(d,800*600))return false;
 d->layer_valid=false;memset(d->raw,0,800*600);float ax=points[p->selected][0],ay=points[p->selected][1]+33;
 float bx=p->preview_x*2.5f,by=(p->preview_y-13)*2.5f+33,px=ax,py=ay;
 for(int k=1;k<=64;k++){
  float t=k/64.f,s=1-t,qx=s*s*ax+2*s*t*200+t*t*bx,qy=s*s*ay+2*s*t*433+t*t*by;
  float dx=qx-px,dy=qy-py,len=dx*dx+dy*dy,inv=len>0?1/len:0;
  int x0=(int)floorf(fminf(px,qx)-7),x1=(int)ceilf(fmaxf(px,qx)+7),y0=(int)floorf(fminf(py,qy)-7),y1=(int)ceilf(fmaxf(py,qy)+7);
  if(x0<0)x0=0;
  if(x1>799)x1=799;
  if(y0<33)y0=33;
  if(y1>565)y1=565;
  for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
   float u=((x+.5f-px)*dx+(y+.5f-py)*dy)*inv;if(u<0)u=0;if(u>1)u=1;
   float ex=x+.5f-px-u*dx,ey=y+.5f-py-u*dy,dist=sqrtf(ex*ex+ey*ey);
   unsigned a=dist<=5.5f?255:dist>=6.5f?0:(unsigned)((6.5f-dist)*255+.5f);
   unsigned at=y*800+x;if(a>d->raw[at])d->raw[at]=(uint8_t)a;
  }
  px=qx;py=qy;
 }
 const uint8_t red[3]={153,0,0};for(unsigned i=0;i<800*600;i++)if(d->raw[i])lume_blend_pixel(d->rgb+i*3,red,d->raw[i]);
 return true;
}
/* Keep the current 64 photographed tiles, rather than seeking the whole
   LAN stream after each turn. The bound is independent of play duration;
   switching puzzle releases every tile and its source identity. */
typedef struct {uint8_t *rgba;unsigned frame,w,h;int x,y;} LumePuzzleTile;
static LumePuzzleTile lume_puzzle_tiles[64];
static inline void lume_detail_puzzle_cache_close(void) {
 for(unsigned i=0;i<64;i++){free(lume_puzzle_tiles[i].rgba);memset(&lume_puzzle_tiles[i],0,sizeof lume_puzzle_tiles[i]);}
}
static inline bool lume_detail_solar_tile(LumeDetail *d,unsigned cell,unsigned frame) {
 LumePuzzleTile *t=&lume_puzzle_tiles[cell];
 if(!t->rgba||t->frame!=frame) {
  int x,y;unsigned w,h;
  if(!lume_detail_load_layer(d,"solar_tiles",frame,&x,&y,&w,&h))return false;
  size_t bytes=(size_t)w*h*4;
  /* Source cells are about 50 pixels across; reject an accidental full
     stage export rather than allocating 64 unbounded images. */
  if(!bytes||w>64||h>64)return false;
  uint8_t *pixels=realloc(t->rgba,bytes);if(!pixels)return false;
  t->rgba=pixels;memcpy(pixels,d->raw,bytes);t->frame=frame;t->x=x;t->y=y;t->w=w;t->h=h;
 }
 for(unsigned yy=0;yy<t->h;yy++)for(unsigned xx=0;xx<t->w;xx++) {
  int x=t->x+(int)xx,y=t->y+(int)yy;
  if((unsigned)x>=800||(unsigned)y>=600)continue;
  const uint8_t *q=t->rgba+(yy*t->w+xx)*4;
  if(q[3])lume_blend_pixel(d->rgb+(y*800+x)*3,q,q[3]);
 }
 return true;
}
static inline bool lume_detail_puzzle(LumeDetail *d,int mode,const LumePuzzles *p,const unsigned piano[9]) {
 static const char *const names[]={"wires","piano","solar","keys","cupboard","map","door"};
 if(mode<0||mode>=LUME_PUZZLE_COUNT)return false;
 unsigned key=(unsigned)mode+1;
 if(mode==LUME_WIRES){for(int i=0;i<12;i++)key=lume_detail_hash(key,p->wire.links[i]);key=lume_detail_hash(key,p->wire.starts);key=lume_detail_hash(key,p->wire.selected);key=lume_detail_hash(key,p->wire.preview_x);key=lume_detail_hash(key,p->wire.preview_y);}
 if(mode==LUME_KEYS)key=lume_detail_hash(key,p->keys.selected);
 if(mode==LUME_SOLAR)for(int i=0;i<64;i++)key=lume_detail_hash(key,p->solar.turns[i]&3);
 if(mode==LUME_MAP){key=lume_detail_hash(key,p->map.x);key=lume_detail_hash(key,p->map.y);key=lume_detail_hash(key,p->map.ink_variant);}
 const LockPuzzle *lock=mode==LUME_CUPBOARD?&p->cupboard:&p->door;
 if(mode==LUME_CUPBOARD||mode==LUME_DOOR){for(int i=0;i<3;i++)key=lume_detail_hash(key,lock->digits[i]);key=lume_detail_hash(key,lock->stage);}
 if(mode==LUME_PIANO)for(int i=0;i<9;i++)key=lume_detail_hash(key,piano[i]);
 int changed=lume_detail_board_begin(d,names[mode],key);if(changed!=1)return changed==0;
 bool ok=true;
 if(mode==LUME_KEYS)for(unsigned i=0;i<14;i++)if(p->keys.selected&(1u<<i))ok=lume_detail_layer(d,"art_keys",i)&&ok;
 if(mode==LUME_WIRES){
  for(unsigned i=0;i<12;i++){int state=p->wire.selected==(int)i||(p->wire.starts&(1u<<i))?1:p->wire.links[i]?2:0;if(state)ok=lume_detail_layer(d,"art_wires",i*2+state-1)&&ok;}
  unsigned pair=24;for(unsigned i=0;i<12;i++)for(unsigned j=i+1;j<12;j++,pair++)if(p->wire.links[i]==(int)j+1)ok=lume_detail_layer(d,"art_wires",pair)&&ok;
  ok=lume_detail_wire_preview(d,&p->wire)&&ok;
 }
 if(mode==LUME_SOLAR)for(unsigned i=0;i<64;i++)ok=lume_detail_solar_tile(d,i,i*4+(p->solar.turns[i]&3))&&ok;
 if(mode==LUME_PIANO)for(unsigned i=0;i<9;i++)if(piano[i]!=~0u)ok=lume_detail_layer(d,"ui_piano_keys",(i==8?216:i*27)+piano[i])&&ok;
 if(mode==LUME_CUPBOARD||mode==LUME_DOOR){
  for(unsigned i=0;i<3;i++)ok=lume_detail_layer(d,mode==LUME_CUPBOARD?"digits_cupboard":"digits_door",i*10+(unsigned)lock->digits[i])&&ok;
  if(mode==LUME_CUPBOARD&&lock->stage>0&&lock->stage<=3)ok=lume_detail_layer(d,"ui_cupboard_progress",lock->stage-1)&&ok;
 }
 if(mode==LUME_MAP&&p->map.x>=0&&p->map.x<10&&p->map.y>=0&&p->map.y<10){
  float gx=p->map.x*40+20,gy=p->map.y*40+20,px=56.85f+.75f*gx,py=7.6f+.75f*gy;
  int x=(int)lroundf(.989837646f*px+.095306396f*py+9.25f),y=(int)lroundf(-.095321655f*px+.989837646f*py+113.55f+33);
  ok=lume_detail_layer_shift(d,"map_ink",p->map.ink_variant&3u,x-400,y-300)&&ok;
 }
 d->valid=ok;return ok;
}
#endif
