#ifndef SAMOROST_TIMELINE_H
#define SAMOROST_TIMELINE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "engine/animation/baked.h"
#define SM_IDS 8192
#define SM_NODES 2048
#define SM_CHILDREN 64
#define SM_STRINGS 512
#define SM_TEXTURES 8
#define SM_MESHES 32
typedef struct {float a,b,c,d,x,y;} SmMatrix;
typedef struct {char *name;unsigned original_w,original_h,w,h;} SmTexture;
typedef struct {unsigned id,texture,x,y,w,h;int8_t *relief;} SmImage;
typedef struct {unsigned id,size,frames;const uint8_t *offsets,*data;const LeBakedFrame **baked_frames;} SmSprite;
typedef struct {unsigned first,pairs,triangle,triangles;} SmStrokeRun;
typedef struct {unsigned id,buffer,first,triangles,stroke_first,stroke_pairs,stroke_count;SmStrokeRun *strokes;} SmShape;
typedef struct {unsigned count,indices;float *vertices;uint16_t *index;} SmMesh;
typedef struct {unsigned id;char *name;} SmSound;
typedef struct {
 LeBakedAnimation baked;
 char name[32];unsigned root;uint8_t *bytes;size_t size;
 char *strings[SM_STRINGS],*names[SM_IDS];SmTexture textures[SM_TEXTURES];unsigned texture_count;
 SmImage *images;SmSprite *sprites;SmShape *shapes;SmSound *sounds;SmMesh mesh[SM_MESHES];
 unsigned image_count,sprite_count,shape_count,sound_count,invalid_vertices,depth_count;
 int16_t image_index[SM_IDS],sprite_index[SM_IDS],shape_index[SM_IDS];
} SmAssets;
typedef struct {
 bool alive,visible,playing,callback,static_sprite,loop;
 unsigned generation,born,id,kind,frame,pointer,pending,fps,revision;int parent;
 unsigned children_count;int children[SM_CHILDREN];
 float elapsed,alpha,color[8],depth,width,height;SmMatrix transform;char name[128];SmAssets *assets;
} SmNode;
struct SmVm;
typedef void (*SmEvent)(struct SmVm*,int,const char*,void*);
typedef void (*SmNamed)(struct SmVm*,int,void*);
typedef void (*SmAudio)(struct SmVm*,SmAssets*,unsigned,float,float,void*);
typedef struct SmVm {
 SmAssets *common,*scene;SmNode nodes[SM_NODES];unsigned tick,serial,count,random,baked_steps,interpreted_steps;int root;
 bool failed,seeking,baked_enabled;char error[160];SmEvent event;SmNamed named;SmAudio sound;void *user;
} SmVm;
bool sm_assets_load(SmAssets*,const char*,const char*);
void sm_assets_free(SmAssets*);
bool sm_baked_load(SmAssets*,const char*);
void sm_baked_clear(SmAssets*);
void sm_vm_init(SmVm*,SmAssets*,SmAssets*,SmEvent,SmNamed,SmAudio,void*);
void sm_vm_clear(SmVm*);
int sm_create(SmVm*,unsigned,unsigned,int);
void sm_tick(SmVm*,float);
void sm_seek(SmVm*,int,unsigned,bool);
void sm_seek_label(SmVm*,int,const char*,bool);
void sm_play(SmVm*,int);
void sm_stop(SmVm*,int);
void sm_next(SmVm*,int);
int sm_container(SmVm*);
int sm_rectangle(SmVm*,float,float);
void sm_remove(SmVm*,int);
void sm_attach(SmVm*,int,int,unsigned);
int sm_find(SmVm*,const char*);
int sm_find_child(SmVm*,int,const char*);
int sm_child(SmVm*,int,unsigned);
unsigned sm_frame(SmVm*,int);
SmMatrix sm_identity(void);
SmMatrix sm_multiply(SmMatrix,SmMatrix);
bool sm_inverse_point(SmMatrix,float*,float*);
SmMatrix sm_world(SmVm*,int);
const char *sm_string(SmVm*,SmAssets*,unsigned);
SmImage *sm_image(SmAssets*,unsigned);
SmShape *sm_shape(SmAssets*,unsigned);
#endif