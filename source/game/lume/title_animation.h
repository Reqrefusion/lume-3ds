#ifndef LUME_TITLE_ANIMATION_H
#define LUME_TITLE_ANIMATION_H
#include <stdint.h>
#include "../../engine/render/colour.h"
#include <string.h>
#include <stdbool.h>
/* Source menuAnim: frames 1..733, three remaining child ticks, then a
   365-frame particle loop. animOut is 734..776 and hands off once, at its end. */
enum { LUME_TITLE_ENTRY, LUME_TITLE_IDLE, LUME_TITLE_EXIT };
static inline unsigned lume_title_phase(uint64_t ticks) {
  return ticks < 736 ? LUME_TITLE_ENTRY : LUME_TITLE_IDLE;
}
static inline unsigned lume_title_frame(uint64_t ticks) {
  return ticks < 736 ? (unsigned)ticks : (unsigned)((ticks - 736) % 365);
}
/* Source menuAnim128 fades logo64 from frame195 through234. Fade the
   lower menu on that same25Hz clock; input still waits for btnCoverUp. */
static inline unsigned lume_title_menu_opacity(uint64_t ticks) {
  if(ticks<=195)return 0;
  if(ticks>=234)return 255;
  return (unsigned)(((ticks-195)*255+19)/39);
}
static inline void lume_title_menu_blend(uint8_t *menu,const uint8_t *scene,
                                          unsigned bytes,unsigned alpha) {
  if(alpha>=255)return;
  if(!alpha){memcpy(menu,scene,bytes);return;}
  unsigned i=0;
  for(;i+2<bytes;i+=3)lume_blend_pixel(menu+i,scene+i,255-alpha);
  for(;i<bytes;i++)menu[i]=(menu[i]*alpha+scene[i]*(255-alpha)+127)/255;
}
static inline bool lume_title_touch_ready(uint64_t ticks) {
  // Original btnCoverUp is removed at source frame 238.
  return ticks>=237;
}
/* Studio plays once on the bottom screen; the normal menu fade keeps its
   authored clock and input becomes available at the original tick. */
static inline unsigned lume_title_studio_frame(uint64_t ticks,unsigned count){
 return !count?0:ticks<count?(unsigned)ticks:count-1;
}
static inline unsigned lume_title_music_percent(uint64_t elapsed_ms) {
  /* Original outer timeline increases music volume by 0.5 at 25 Hz. */
  return elapsed_ms >= 8000 ? 100 : (unsigned)(elapsed_ms / 80);
}
#endif
