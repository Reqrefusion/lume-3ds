#include "engine/adapters/sprite_timeline/timeline.h"
#include "engine/adapters/sprite_timeline/stroke.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "engine/assets/file_cache.h"
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static unsigned le16(const uint8_t *p){return (unsigned)p[0]|(unsigned)p[1]<<8;}
typedef struct {const uint8_t *p,*end;bool ok;} Cursor;
static const uint8_t *take(Cursor *r,size_t n){if(!r->ok||n>(size_t)(r->end-r->p)){r->ok=false;return NULL;}const uint8_t *p=r->p;r->p+=n;return p;}
static unsigned u32(Cursor *r){const uint8_t *p=take(r,4);return p?le32(p):0;}
static char *text(Cursor *r){unsigned n=u32(r);if(n>1024){r->ok=false;return NULL;}const uint8_t *p=take(r,n);if(!p)return NULL;char *s=malloc(n+1);if(!s){r->ok=false;return NULL;}memcpy(s,p,n);s[n]=0;return s;}
void sm_baked_clear(SmAssets *a){
 if(a->sprites)for(unsigned i=0;i<a->sprite_count;i++){free(a->sprites[i].baked_frames);a->sprites[i].baked_frames=NULL;}
 le_baked_free(&a->baked);
}
bool sm_baked_load(SmAssets *a,const char *path){
 sm_baked_clear(a);if(!le_baked_load(&a->baked,path,a->bytes,a->size,SM_CHILDREN))return false;
 size_t map_bytes=0;
 for(unsigned i=0;i<a->baked.frame_count;i++){
  const LeBakedFrame *frame=a->baked.frames+i;SmSprite *clip=NULL;
  for(unsigned j=0;j<a->sprite_count;j++)if(a->sprites[j].id==frame->clip){clip=a->sprites+j;break;}
  if(!clip||frame->frame>=clip->frames||frame->pointer!=le32(clip->offsets+frame->frame*4)||frame->next>clip->size)goto fail;
  if(!clip->baked_frames){
   map_bytes+=(size_t)clip->frames*sizeof *clip->baked_frames;if(map_bytes>128*1024)goto fail;
   clip->baked_frames=calloc(clip->frames,sizeof *clip->baked_frames);if(!clip->baked_frames)goto fail;
  }
  clip->baked_frames[frame->frame]=frame;
 }
 return true;
fail:sm_baked_clear(a);return false;
}
void sm_assets_free(SmAssets *a){
 sm_baked_clear(a);
 for(unsigned i=0;i<SM_STRINGS;i++)free(a->strings[i]);
 for(unsigned i=0;i<SM_IDS;i++)free(a->names[i]);
 for(unsigned i=0;i<SM_TEXTURES;i++)free(a->textures[i].name);
 if(a->sounds)for(unsigned i=0;i<a->sound_count;i++)free(a->sounds[i].name);
 for(unsigned i=0;i<SM_MESHES;i++){free(a->mesh[i].vertices);free(a->mesh[i].index);}
 if(a->images)for(unsigned i=0;i<a->image_count;i++)free(a->images[i].relief);
 if(a->shapes)for(unsigned i=0;i<a->shape_count;i++)free(a->shapes[i].strokes);
 free(a->bytes);free(a->images);free(a->sprites);free(a->shapes);free(a->sounds);memset(a,0,sizeof *a);
}
bool sm_assets_load(SmAssets *a,const char *path,const char *name){
 memset(a,0,sizeof *a);memset(a->image_index,0xff,sizeof a->image_index);memset(a->sprite_index,0xff,sizeof a->sprite_index);memset(a->shape_index,0xff,sizeof a->shape_index);
 snprintf(a->name,sizeof a->name,"%s",name);FILE *f=fopen(path,"rb");if(!f)return false;
 if(fseek(f,0,SEEK_END)){fclose(f);return false;}long length=ftell(f);rewind(f);
 if(length<44||length>8*1024*1024){fclose(f);return false;}a->size=(size_t)length;a->bytes=malloc(a->size);LumeFileCache cache={0};bool ok=a->bytes&&lume_cache_init(&cache,f,(uint32_t)a->size);
 for(unsigned at=0;ok&&at<a->size;){unsigned n=(unsigned)a->size-at;if(n>32768)n=32768;const uint8_t *p=lume_cache_get(&cache,at,n);if(!p){ok=false;break;}memcpy(a->bytes+at,p,n);at+=n;}
 lume_cache_close(&cache);fclose(f);if(!ok){sm_assets_free(a);return false;}
 unsigned bad_line=0;
 Cursor r={a->bytes,a->bytes+a->size,true};const uint8_t *magic=take(&r,4);if(!magic||memcmp(magic,"SPK1",4)||u32(&r)!=1){bad_line=__LINE__;goto fail;}
 a->root=u32(&r);unsigned ns=u32(&r);a->texture_count=u32(&r);a->image_count=u32(&r);a->sprite_count=u32(&r);a->shape_count=u32(&r);unsigned meshes=u32(&r);a->sound_count=u32(&r);unsigned names=u32(&r);
 if(ns>SM_STRINGS||a->texture_count>SM_TEXTURES||a->image_count>SM_IDS||a->sprite_count>SM_IDS||a->shape_count>SM_IDS||meshes>SM_MESHES||a->sound_count>SM_IDS||names>SM_IDS||a->root>=SM_IDS){bad_line=__LINE__;goto fail;}
 a->images=calloc(a->image_count?a->image_count:1,sizeof *a->images);a->sprites=calloc(a->sprite_count?a->sprite_count:1,sizeof *a->sprites);a->shapes=calloc(a->shape_count?a->shape_count:1,sizeof *a->shapes);a->sounds=calloc(a->sound_count?a->sound_count:1,sizeof *a->sounds);
 if(!a->images||!a->sprites||!a->shapes||!a->sounds){bad_line=__LINE__;goto fail;}
 for(unsigned i=0;i<ns;i++){unsigned id=u32(&r);if(id>=SM_STRINGS||a->strings[id]){bad_line=__LINE__;goto fail;}a->strings[id]=text(&r);}
 for(unsigned i=0;i<a->texture_count;i++){SmTexture *t=a->textures+i;t->name=text(&r);t->original_w=u32(&r);t->original_h=u32(&r);t->w=u32(&r);t->h=u32(&r);if(!t->name||strchr(t->name,'/')||strchr(t->name,'\\')||strstr(t->name,"..")||t->w>1024||t->h>1024||!t->w||!t->h){bad_line=__LINE__;goto fail;}}
 for(unsigned i=0;i<a->image_count;i++){SmImage *d=a->images+i;d->id=u32(&r);d->texture=u32(&r);d->x=u32(&r);d->y=u32(&r);d->w=u32(&r);d->h=u32(&r);if(d->id>=SM_IDS||d->texture>=a->texture_count||a->image_index[d->id]>=0||d->x+d->w>a->textures[d->texture].original_w||d->y+d->h>a->textures[d->texture].original_h){bad_line=__LINE__;goto fail;}a->image_index[d->id]=(int16_t)i;}
 for(unsigned i=0;i<a->sprite_count;i++){SmSprite *s=a->sprites+i;s->id=u32(&r);s->size=u32(&r);s->frames=u32(&r);if(s->id>=SM_IDS||a->sprite_index[s->id]>=0||s->size<3||s->frames>65536){bad_line=__LINE__;goto fail;}s->offsets=take(&r,s->frames*4);s->data=take(&r,s->size);if(!s->offsets||!s->data){bad_line=__LINE__;goto fail;}for(unsigned j=0;j<s->frames;j++)if(le32(s->offsets+j*4)>=s->size){bad_line=__LINE__;goto fail;}a->sprite_index[s->id]=(int16_t)i;}
 for(unsigned i=0;i<a->shape_count;i++){SmShape *s=a->shapes+i;s->id=u32(&r);s->buffer=u32(&r);s->first=u32(&r);s->triangles=u32(&r);if(s->id>=SM_IDS||s->buffer>=SM_MESHES||a->shape_index[s->id]>=0){bad_line=__LINE__;goto fail;}a->shape_index[s->id]=(int16_t)i;}
 for(unsigned i=0;i<meshes;i++){unsigned id=u32(&r),nv=u32(&r),ni=u32(&r);if(id>=SM_MESHES||nv>262144||ni>1048576||ni%3||a->mesh[id].vertices){bad_line=__LINE__;goto fail;}SmMesh *m=a->mesh+id;m->count=nv;m->indices=ni;const uint8_t *v=take(&r,(size_t)nv*24),*ix=take(&r,(size_t)ni*2);if(!v||!ix){bad_line=__LINE__;goto fail;}m->vertices=malloc((size_t)nv*24);m->index=malloc((size_t)ni*2);if(!m->vertices||!m->index){bad_line=__LINE__;goto fail;}memcpy(m->vertices,v,(size_t)nv*24);memcpy(m->index,ix,(size_t)ni*2);for(unsigned j=0;j<ni;j++)if(m->index[j]>=nv){bad_line=__LINE__;goto fail;}for(unsigned j=0;j<nv;j++){if(!isfinite(m->vertices[j*6])||!isfinite(m->vertices[j*6+1]))a->invalid_vertices++;for(unsigned c=2;c<6;c++)if(!isfinite(m->vertices[j*6+c])){bad_line=__LINE__;goto fail;}}}
 for(unsigned i=0;i<a->shape_count;i++){SmShape *s=a->shapes+i;SmMesh *m=a->mesh+s->buffer;if(s->first>m->indices||s->triangles>(m->indices-s->first)/3){bad_line=__LINE__;goto fail;}sm_stroke_classify(s,m);}
 for(unsigned i=0;i<a->sound_count;i++){a->sounds[i].id=u32(&r);a->sounds[i].name=text(&r);}
 for(unsigned i=0;i<names;i++){unsigned id=u32(&r);if(id>=SM_IDS||a->names[id]){bad_line=__LINE__;goto fail;}a->names[id]=text(&r);}
 if(!r.ok||r.p!=r.end){bad_line=__LINE__;goto fail;}
 size_t path_size=strlen(path);if(path_size>4&&path_size<1000&&!strcmp(path+path_size-4,".spk")){
  char baked_path[1024];snprintf(baked_path,sizeof baked_path,"%.*s.lbk",(int)path_size-4,path);sm_baked_load(a,baked_path);
 }
 return true;
fail:fprintf(stderr,"Invalid asset %s at reader line %u\n",name,bad_line);sm_assets_free(a);return false;
}
SmMatrix sm_identity(void){return (SmMatrix){1,0,0,1,0,0};}
SmMatrix sm_multiply(SmMatrix p,SmMatrix q){return (SmMatrix){p.a*q.a+p.c*q.b,p.b*q.a+p.d*q.b,p.a*q.c+p.c*q.d,p.b*q.c+p.d*q.d,p.a*q.x+p.c*q.y+p.x,p.b*q.x+p.d*q.y+p.y};}
bool sm_inverse_point(SmMatrix m,float *x,float *y){float det=m.a*m.d-m.b*m.c;if(fabsf(det)<1e-8f)return false;float u=*x-m.x,v=*y-m.y;*x=(m.d*u-m.c*v)/det;*y=(-m.b*u+m.a*v)/det;return true;}
SmMatrix sm_world(SmVm *v,int id){SmMatrix m=sm_identity();unsigned guard=0;while(id>=0&&id<SM_NODES&&v->nodes[id].alive&&guard++<64){SmNode *n=v->nodes+id;m=sm_multiply(n->transform,m);id=n->parent;}return m;}
SmImage *sm_image(SmAssets *a,unsigned id){return a&&id<SM_IDS&&a->image_index[id]>=0?a->images+a->image_index[id]:NULL;}
SmShape *sm_shape(SmAssets *a,unsigned id){return a&&id<SM_IDS&&a->shape_index[id]>=0?a->shapes+a->shape_index[id]:NULL;}
static SmSprite *sprite(SmAssets *a,unsigned id){return a&&id<SM_IDS&&a->sprite_index[id]>=0?a->sprites+a->sprite_index[id]:NULL;}
const char *sm_string(SmVm *v,SmAssets *a,unsigned id){if(id>=SM_STRINGS)return "";return a&&a->strings[id]?a->strings[id]:v->common&&v->common->strings[id]?v->common->strings[id]:"";}
static void error(SmVm *v,const char *why){v->failed=true;snprintf(v->error,sizeof v->error,"%s",why);}
static void reset_color(SmNode *n){n->alpha=1;for(unsigned i=0;i<8;i++)n->color[i]=i<4?1:0;}
static void remove_node(SmVm *v,int id){if(id<0||id>=SM_NODES||!v->nodes[id].alive)return;SmNode *n=v->nodes+id;for(unsigned i=0;i<n->children_count;i++)remove_node(v,n->children[i]);n->alive=false;n->children_count=0;}
static void clear_children(SmVm *v,SmNode *n){for(unsigned i=0;i<n->children_count;i++)remove_node(v,n->children[i]);n->children_count=0;}
static void step(SmVm*,int,bool,bool);
static int allocate_node(SmVm *v){for(unsigned i=0;i<SM_NODES;i++)if(!v->nodes[i].alive){SmNode *n=v->nodes+i;unsigned generation=n->generation+1;memset(n,0,sizeof *n);n->alive=n->visible=n->loop=true;n->generation=generation;n->parent=-1;n->born=v->tick;n->transform=sm_identity();n->depth=NAN;reset_color(n);v->count++;return (int)i;}error(v,"Timeline node capacity exceeded");return -1;}
int sm_create(SmVm *v,unsigned kind,unsigned id,int parent){
 SmAssets *a=v->scene;bool found=kind==0?sm_image(a,id)!=NULL:kind==1?sprite(a,id)!=NULL:sm_shape(a,id)!=NULL;
 if(!found){a=v->common;found=kind==0?sm_image(a,id)!=NULL:kind==1?sprite(a,id)!=NULL:sm_shape(a,id)!=NULL;}
 if(!found){error(v,"Unknown timeline asset");return -1;}
 int node=allocate_node(v);if(node<0)return -1;SmNode *n=v->nodes+node;n->assets=a;n->id=id;n->kind=kind;n->parent=parent;n->fps=25;n->pending=SM_STRINGS;
 if(kind==1){SmSprite *s=sprite(a,id);n->static_sprite=le16(s->data)==1;n->playing=!n->static_sprite;n->pointer=2;step(v,node,false,false);}
 return node;
}
void sm_vm_init(SmVm *v,SmAssets *common,SmAssets *scene,SmEvent event,SmNamed named,SmAudio audio,void *user){memset(v,0,sizeof *v);v->common=common;v->scene=scene;v->event=event;v->named=named;v->sound=audio;v->user=user;v->random=0x51a3u;v->root=-1;v->baked_enabled=true;}
void sm_vm_clear(SmVm *v){for(unsigned i=0;i<SM_NODES;i++)v->nodes[i].alive=false;v->root=-1;v->count=0;v->failed=false;}
static unsigned rnd(SmVm *v,unsigned maximum){v->random=v->random*1664525u+1013904223u;return maximum?v->random%(maximum+1):0;}
static float f32(Cursor *r){const uint8_t *p=take(r,4);float f=0;if(p)memcpy(&f,p,4);if(!isfinite(f))r->ok=false;return f;}
static unsigned b8(Cursor *r){const uint8_t *p=take(r,1);return p?*p:0;}
static unsigned b16(Cursor *r){const uint8_t *p=take(r,2);return p?le16(p):0;}
static void named(SmVm *v,int id){if(v->named&&!v->seeking&&id>=0&&v->nodes[id].alive&&v->nodes[id].name[0])v->named(v,id,v->user);}
static void step(SmVm *v,int id,bool reuse,bool mute){
 if(v->failed||id<0||id>=SM_NODES)return;SmNode *n=v->nodes+id;if(!n->alive||n->kind!=1)return;SmSprite *s=sprite(n->assets,n->id);if(!s||n->pointer>=s->size){error(v,"Timeline cursor outside definition");return;}
 if(v->baked_enabled&&s->baked_frames&&n->frame<s->frames){
  const LeBakedFrame *frame=s->baked_frames[n->frame];
  if(frame&&frame->pointer==n->pointer){
   for(unsigned i=0;i<frame->count;i++){
    const LeBakedMove *move=n->assets->baked.moves+frame->first+i;
    if(move->slot>=n->children_count){error(v,"Move references missing child");return;}
    SmNode *child=v->nodes+n->children[move->slot];const float *p=move->value;unsigned mask=move->mask;
    if(mask&8){child->transform.x=p[0];child->transform.y=p[1];}
    if(mask&16){child->transform.a=p[2];child->transform.d=p[3];}
    if(mask&32){child->transform.b=p[4];child->transform.c=p[5];}
    if(mask&64){memcpy(child->color,p+6,8*sizeof(float));child->alpha=1;}
    else if(mask&128)child->alpha=p[14];
   }
   n->pointer=frame->next;n->frame++;n->revision++;v->baked_steps++;return;
  }
 }
 v->interpreted_steps++;
 Cursor r={s->data+n->pointer,s->data+s->size,true};unsigned limit=0,maxchild=0;bool placed=false,done=false;
 while(r.ok&&!done&&limit++<16384){unsigned op=b8(&r);
  if(op==128){done=true;break;}
  if(op==0){if(n->static_sprite||!n->loop){n->playing=false;done=true;break;}n->frame=0;n->pointer=2;step(v,id,true,mute);return;}
  if(op==1||op==5||op==10||op==2){unsigned name_id=SM_STRINGS;if(op==10)name_id=b8(&r);unsigned object=op==2?0:b16(&r),depth=b16(&r),flags=b8(&r);if(depth>=SM_CHILDREN){error(v,"Timeline child index exceeds capacity");return;}
   int child=-1;bool created=false;
   if(op==2){if(depth>=n->children_count){error(v,"Move references missing child");return;}child=n->children[depth];}
   else {
    if(reuse&&depth<n->children_count){int old=n->children[depth];if(v->nodes[old].id==object&&v->nodes[old].kind==(flags&7))child=old;}
    if(child<0){child=sm_create(v,flags&7,object,id);created=true;if(child<0)return;}
    if(created){
     if(op==5||reuse){if(depth<n->children_count){remove_node(v,n->children[depth]);n->children[depth]=child;}else if(depth==n->children_count)n->children[n->children_count++]=child;else{error(v,"Replace references missing child slot");return;}}
     else {if(depth>n->children_count||n->children_count>=SM_CHILDREN){error(v,"Invalid timeline insertion");return;}memmove(n->children+depth+1,n->children+depth,(n->children_count-depth)*sizeof *n->children);n->children[depth]=child;n->children_count++;}
    }
    placed=true;if(depth+1>maxchild)maxchild=depth+1;
   }
   SmNode *c=v->nodes+child;if(op!=2){c->transform=sm_identity();reset_color(c);}
   if(flags&8){c->transform.x=f32(&r);c->transform.y=f32(&r);}if(flags&16){c->transform.a=f32(&r);c->transform.d=f32(&r);}if(flags&32){c->transform.b=f32(&r);c->transform.c=f32(&r);}
   if(flags&64){for(unsigned j=0;j<8;j++)c->color[j]=f32(&r);c->alpha=1;}else if(flags&128)c->alpha=b8(&r)/255.0f;
   if(op==10){snprintf(c->name,sizeof c->name,"%s",sm_string(v,n->assets,name_id));if(created)named(v,child);}
  }else if(op==3){unsigned depth=b16(&r);if(depth>=n->children_count){error(v,"Remove references missing child");return;}remove_node(v,n->children[depth]);memmove(n->children+depth,n->children+depth+1,(n->children_count-depth-1)*sizeof *n->children);n->children_count--;
  }else if(op==4)n->pending=b8(&r);
  else if(op==6){unsigned sound=b16(&r);b16(&r);b8(&r);float left=b16(&r)/32768.0f,right=b16(&r)/32768.0f;if(!mute&&v->sound)v->sound(v,n->assets,sound,left,right,v->user);}
  else if(op==7)n->callback=true;
  else if(op==8||op==9){b8(&r);if(op==8)b8(&r);if(b8(&r))b8(&r);}
  else if(op==30){unsigned fps=b8(&r);n->fps=fps?fps:1;}
  else if(op==31){unsigned base=b8(&r),range=b8(&r);n->fps=base+rnd(v,range);if(!n->fps)n->fps=1;}
  else if(op==32)n->fps=1+rnd(v,b8(&r));
  else if(op==33)n->playing=false;
  else if(op==34){sm_remove(v,id);return;}
  else if(op==35){}
  else if(op==36)n->visible=false;
  else if(op==37)n->visible=true;
  else {error(v,"Unknown timeline opcode");return;}
 }
 if(!r.ok||limit>=16384){error(v,"Truncated timeline instructions");return;}
 if(reuse&&placed)while(n->children_count>maxchild)remove_node(v,n->children[--n->children_count]);
 n->pointer=(unsigned)(r.p-s->data);n->frame++;n->revision++;
}
void sm_play(SmVm *v,int id){if(id>=0&&id<SM_NODES&&v->nodes[id].alive){v->nodes[id].playing=!v->nodes[id].static_sprite;v->nodes[id].pending=SM_STRINGS;v->nodes[id].elapsed=0;}}
void sm_stop(SmVm *v,int id){if(id>=0&&id<SM_NODES)v->nodes[id].playing=false;}
static void notify_tree(SmVm *v,int id){if(id<0||!v->nodes[id].alive)return;named(v,id);SmNode *n=v->nodes+id;for(unsigned i=0;i<n->children_count;i++)notify_tree(v,n->children[i]);}
void sm_seek(SmVm *v,int id,unsigned frame,bool play){
 if(id<0||id>=SM_NODES||!v->nodes[id].alive||v->nodes[id].kind!=1)return;SmNode *n=v->nodes+id;SmSprite *s=sprite(n->assets,n->id);unsigned total=le16(s->data);if(frame<1)frame=1;if(frame>total)frame=total;
 if(n->frame==frame){n->playing=play&&!n->static_sprite;return;}
 bool old=v->seeking;v->seeking=true;
 if(frame<n->frame){clear_children(v,n);n->frame=0;n->pointer=2;}
 n->pending=SM_STRINGS;n->elapsed=0;n->playing=true;
 unsigned count=frame-n->frame;for(unsigned i=0;i<count&&!v->failed&&n->alive;i++)step(v,id,false,true);
 n->playing=play&&!n->static_sprite;n->revision++;v->seeking=old;if(!old)for(unsigned i=0;i<n->children_count;i++)notify_tree(v,n->children[i]);
}
void sm_next(SmVm *v,int id){if(id>=0&&id<SM_NODES&&v->nodes[id].alive){SmNode *n=v->nodes+id;SmSprite *s=sprite(n->assets,n->id);if(s&&n->frame<le16(s->data))step(v,id,false,false);}}
void sm_seek_label(SmVm *v,int id,const char *label,bool play){
 if(id<0||id>=SM_NODES||!v->nodes[id].alive)return;
 SmNode *n=v->nodes+id;SmSprite *s=sprite(n->assets,n->id);if(!s)return;
 for(unsigned f=0;f<s->frames;f++){
  Cursor r={s->data+le32(s->offsets+f*4),s->data+s->size,true};
  while(r.ok&&r.p<r.end){unsigned op=b8(&r);
   if(op==0||op==128)break;
   if(op==4){unsigned name=b8(&r);if(!strcmp(sm_string(v,n->assets,name),label)){sm_seek(v,id,f+1,play);return;}}
   else if(op==1||op==2||op==5||op==10){if(op==10)b8(&r);if(op!=2)b16(&r);b16(&r);unsigned flags=b8(&r);take(&r,((flags&8)?8:0)+((flags&16)?8:0)+((flags&32)?8:0)+((flags&64)?32:((flags&128)?1:0)));}
   else if(op==3)take(&r,2);
   else if(op==6)take(&r,9);
   else if(op==8||op==9){b8(&r);if(op==8)b8(&r);if(b8(&r))b8(&r);}
   else if(op==30||op==32)b8(&r);
   else if(op==31)take(&r,2);
   else if(op!=7&&(op<33||op>37))break;
  }
 }
}
static void detach(SmVm *v,int id){
 int parent=v->nodes[id].parent;if(parent>=0&&parent<SM_NODES){SmNode *p=v->nodes+parent;for(unsigned i=0;i<p->children_count;i++)if(p->children[i]==id){memmove(p->children+i,p->children+i+1,(p->children_count-i-1)*sizeof(int));p->children_count--;break;}}
 v->nodes[id].parent=-1;
}
int sm_container(SmVm *v){int id=allocate_node(v);if(id>=0){SmNode *n=v->nodes+id;n->kind=1;n->static_sprite=true;}return id;}
int sm_rectangle(SmVm *v,float width,float height){int id=allocate_node(v);if(id>=0){v->nodes[id].kind=3;v->nodes[id].width=width;v->nodes[id].height=height;}return id;}
void sm_remove(SmVm *v,int id){if(id<0||id>=SM_NODES||!v->nodes[id].alive)return;detach(v,id);remove_node(v,id);}
void sm_attach(SmVm *v,int id,int parent,unsigned index){
 if(id<0||parent<0||id>=SM_NODES||parent>=SM_NODES||!v->nodes[id].alive||!v->nodes[parent].alive||id==parent)return;
 for(int p=parent;p>=0;p=v->nodes[p].parent)if(p==id)return;
 detach(v,id);SmNode *p=v->nodes+parent;if(p->children_count>=SM_CHILDREN){error(v,"Container child capacity exceeded");return;}
 if(index>p->children_count)index=p->children_count;memmove(p->children+index+1,p->children+index,(p->children_count-index)*sizeof(int));p->children[index]=id;p->children_count++;v->nodes[id].parent=parent;
}

int sm_find_child(SmVm *v,int id,const char *name){if(id<0||id>=SM_NODES||!v->nodes[id].alive)return -1;SmNode *n=v->nodes+id;for(unsigned i=0;i<n->children_count;i++){int child=n->children[i];if(!strcmp(v->nodes[child].name,name))return child;}for(unsigned i=0;i<n->children_count;i++){int child=sm_find_child(v,n->children[i],name);if(child>=0)return child;}return -1;}
int sm_find(SmVm *v,const char *name){return sm_find_child(v,v->root,name);}
int sm_child(SmVm *v,int id,unsigned index){return id>=0&&id<SM_NODES&&v->nodes[id].alive&&index<v->nodes[id].children_count?v->nodes[id].children[index]:-1;}
unsigned sm_frame(SmVm *v,int id){return id>=0&&id<SM_NODES&&v->nodes[id].alive?v->nodes[id].frame:0;}
void sm_tick(SmVm *v,float dt){
 v->tick++;v->serial++;if(!isfinite(dt)||dt<0)return;if(dt>0.2f)dt=0.2f;
 for(unsigned id=0;id<SM_NODES&&!v->failed;id++){SmNode *n=v->nodes+id;if(!n->alive||n->born>=v->tick||!n->playing||n->kind!=1)continue;unsigned generation=n->generation;n->elapsed+=dt;unsigned guard=0;
  while(n->alive&&n->generation==generation&&n->playing&&n->elapsed>=1.0f/n->fps&&guard++<16){n->elapsed-=1.0f/n->fps;unsigned revision=n->revision;
   if(n->callback&&n->pending<SM_STRINGS&&v->event){const char *label=sm_string(v,n->assets,n->pending);n->pending=SM_STRINGS;v->event(v,(int)id,label,v->user);if(!n->alive||n->generation!=generation||n->revision!=revision||!n->playing)break;}
   step(v,(int)id,false,false);
  }
 }
}