#ifndef SAMOROST_STROKE_H
#define SAMOROST_STROKE_H
#include "engine/adapters/sprite_timeline/timeline.h"
#include <math.h>
#include <stdlib.h>
/* Recognize the source tessellator's paired-edge stroke topology once at load.
   Filled meshes and unrelated triangles retain their original geometry. */
static inline bool sm_stroke_run_thin(const SmMesh *mesh,unsigned base,unsigned pairs){
 float width_sum=0,length=0,previous_x=0,previous_y=0;unsigned narrow=0;
 for(unsigned i=0;i<pairs;i++){
  const float *a=mesh->vertices+(base+i*2)*6,*b=a+6;
  if(!isfinite(a[0])||!isfinite(a[1])||!isfinite(b[0])||!isfinite(b[1]))return false;
  float width=hypotf(b[0]-a[0],b[1]-a[1]),x=(a[0]+b[0])*0.5f,y=(a[1]+b[1])*0.5f;
  if(!isfinite(width))return false;if(width>=0.001f&&width<=12)narrow++;width_sum+=fminf(width,12);
  if(i)length+=hypotf(x-previous_x,y-previous_y);previous_x=x;previous_y=y;
 }
 return narrow*4>=pairs*3&&length>=8*width_sum/pairs;
}
static inline bool sm_stroke_segment(const SmMesh *mesh,unsigned at,unsigned base){
 const uint16_t *ix=mesh->index+at;
 return base+3<mesh->count&&ix[0]==base&&ix[1]==base+2&&ix[2]==base+1&&ix[3]==base+1&&ix[4]==base+2&&ix[5]==base+3;
}
static inline void sm_stroke_classify(SmShape *shape,const SmMesh *mesh){
 shape->stroke_pairs=shape->stroke_first=shape->stroke_count=0;shape->strokes=NULL;
 for(unsigned t=0;t+1<shape->triangles;){
  unsigned at=shape->first+t*3,base=mesh->index[at],segments=0;
  while(t+segments*2+1<shape->triangles&&segments<255&&sm_stroke_segment(mesh,at+segments*6,base+segments*2))segments++;
  if(!segments){t++;continue;}
  unsigned pairs=segments+1;
  if(sm_stroke_run_thin(mesh,base,pairs)){
   SmStrokeRun *runs=realloc(shape->strokes,(shape->stroke_count+1)*sizeof *runs);
   if(!runs){free(shape->strokes);shape->strokes=NULL;shape->stroke_count=shape->stroke_pairs=0;return;}
   shape->strokes=runs;runs[shape->stroke_count++]=(SmStrokeRun){base,pairs,t,segments*2};
   if(!shape->stroke_pairs){shape->stroke_first=base;shape->stroke_pairs=pairs;}
  }
  t+=segments*2;
 }
}
/* Work in display pixels, so mono bottom, both eyes, rotation and zoom agree.
   Paired edges expand around their shared center without opening segment seams. */
static inline void sm_stroke_pair(SmMatrix m,const float *a,const float *b,float *ax,float *ay,float *bx,float *by){
 *ax=m.a*a[0]+m.c*a[1]+m.x;*ay=m.b*a[0]+m.d*a[1]+m.y;
 *bx=m.a*b[0]+m.c*b[1]+m.x;*by=m.b*b[0]+m.d*b[1]+m.y;
 float dx=*bx-*ax,dy=*by-*ay,width=hypotf(dx,dy);
 if(width>0.00001f&&width<1.35f){
  float gain=(1.35f/width-1)*0.5f;
  *ax-=dx*gain;*ay-=dy*gain;*bx+=dx*gain;*by+=dy*gain;
 }
}
#endif
