#ifndef LUME_MEDIA_TOP
#define LUME_MEDIA_TOP 13
#define LUME_MEDIA_BOTTOM 226
#endif
#include "props.h"
#include "../../engine/render/stereo_video.h"
#include "../../engine/render/blit.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
  unsigned w, h;
  float x, y, inv;
  uint8_t *rgba;
} Prop;
static Prop images[8];
static float *transforms;
static unsigned count;
static float world_transforms[36];
static bool world_ready,show_turbine=true,show_solar=true;
void lume_props_set_visibility(bool turbine,bool solar){show_turbine=turbine;show_solar=solar;}
static bool image_open(Prop *p, const char *name) {
  char path[96];
  snprintf(path, sizeof(path), "romfs:/prop_%s.bin", name);
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  uint8_t h[24];
  bool ok = fread(h, 1, 24, f) == 24 && !memcmp(h, "PRP1", 4);
  if (ok) {
    memcpy(&p->w, h + 4, 4);
    memcpy(&p->h, h + 8, 4);
    memcpy(&p->x, h + 12, 4);
    memcpy(&p->y, h + 16, 4);
    memcpy(&p->inv, h + 20, 4);
    ok = p->w > 0 && p->w <= 320 && p->h > 0 && p->h <= 240 && isfinite(p->x) &&
         isfinite(p->y) && isfinite(p->inv) && p->inv > 0;
  }
  if (ok) {
    size_t n = p->w * p->h * 4;
    p->rgba = malloc(n);
    ok = p->rgba && fread(p->rgba, 1, n, f) == n && fgetc(f) == EOF;
  }
  fclose(f);
  return ok;
}
bool lume_props_init(void) {
  bool ok = true;
  for (int i = 0; i < 8; i++) {
    char name[32];
    if (i < 5)
      snprintf(name, sizeof(name), "turbine%d", i + 1);
    else if (i < 7)
      snprintf(name, sizeof(name), "fence%d", i - 5);
    else
      strcpy(name, "solar");
    if (!image_open(&images[i], name))
      ok = false;
  }
  FILE *f=fopen("romfs:/world_props.trf","rb");unsigned h[3];
  world_ready=f&&fread(h,4,3,f)==3&&!memcmp(h,"TRF1",4)&&h[1]==2&&h[2]==3&&
   fread(world_transforms,4,36,f)==36&&fgetc(f)==EOF&&!ferror(f);
  if(f)fclose(f);
  if(world_ready)for(unsigned i=0;i<36;i++)if(!isfinite(world_transforms[i])||fabsf(world_transforms[i])>10000.f)world_ready=false;
  world_ready=world_ready&&ok;return world_ready;
}
void lume_props_close(void) {
  free(transforms);
  transforms = NULL;
  count = 0;
}
void lume_props_quit(void) {
  lume_props_close();world_ready=false;
  for (int i = 0; i < 8; i++) {
    free(images[i].rgba);
    images[i].rgba = NULL;
  }
}
bool lume_props_open(const char *name, unsigned frames) {
  lume_props_close();
  if (frames > 5000)
    return false;
  char path[128];
  snprintf(path, sizeof(path), "romfs:/%s.trf", name);
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  unsigned h[3];
  bool ok = fread(h, 4, 3, f) == 3 && !memcmp(h, "TRF1", 4) && h[1] == frames &&
            h[2] == 3;
  if (ok) {
    size_t n = frames * 18;
    transforms = malloc(n * 4);
    ok = transforms && fread(transforms, 4, n, f) == n && fgetc(f) == EOF;
    if (ok)
      for (size_t i = 0; i < n; i++)
        if (!isfinite(transforms[i]) || fabsf(transforms[i]) > 10000.f) {
          ok = false;
          break;
        }
  }
  fclose(f);
  if (!ok) {
    lume_props_close();
    return false;
  }
  count = frames;
  return true;
}
static void draw_props(uint8_t *rgb,const float *t,int turbine,bool blades,
                       bool turbine_visible,bool solar_visible,int eye) {
  if (turbine < 1)
    turbine = 1;
  if (turbine > 5)
    turbine = 5;
  int ids[3] = {turbine - 1, blades ? 6 : 5, 7};
  for (int i = 0; i < 3; i++, t += 6) {
    if((i==0&&!turbine_visible)||(i==2&&!solar_visible))continue;
    const Prop *p = &images[ids[i]];
    if (!p->rgba)
      continue;
    float m[6] = {.4f * t[0] * p->inv,
                  .4f * t[1] * p->inv,
                  .4f * t[2] * p->inv,
                  .4f * t[3] * p->inv,
                  .4f * (t[0] * p->x + t[2] * p->y + t[4]) -
                      (eye == 2 ? lume_stereo_strength()/256.f : 0.f),
                  .4f * (t[1] * p->x + t[3] * p->y + t[5]) + 13.f};
    lume_blit_affine(rgb, p->rgba, p->w, p->h, m);
  }
}

void lume_props_frame(uint8_t *rgb,unsigned frame,int turbine,bool blades,int eye){
 if(transforms&&frame<count)draw_props(rgb,transforms+frame*18,turbine,blades,show_turbine,show_solar,eye);
}
bool lume_props_world(uint8_t *rgb,unsigned scene,int turbine,bool blades,bool turbine_visible,bool solar_visible,int eye){
 if(!world_ready||scene>1)return false;
 draw_props(rgb,world_transforms+scene*18,turbine,blades,turbine_visible,solar_visible,eye);return true;
}
