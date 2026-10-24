#ifndef LUME_CHILD_EFFECTS_H
#define LUME_CHILD_EFFECTS_H
#include <stdbool.h>
#include <stdint.h>
/* Source children advance at 25 Hz even when their parent is stopped. Use
 * absolute elapsed time, so a slow draw never stretches the authored timing.
 * Each key owns a clock: a second press must not erase another key's tail. */
typedef struct {
  uint64_t started;
  unsigned frame, event;
  bool active;
} LumeChildClock;
static inline void lume_child_start(LumeChildClock *c,uint64_t now) {
  c->started=now;c->frame=~0u;c->event=0;c->active=true;
}
static inline unsigned lume_child_tick(LumeChildClock *c,uint64_t now,unsigned count) {
  if(!c->active || now<c->started)return c->frame=~0u;
  uint64_t frame=(now-c->started)/40;
  if(frame>=count){c->active=false;c->frame=~0u;return ~0u;}
  return c->frame=(unsigned)frame;
}
/* Sprite 886's a_to_gramophone starts at frame 11 and starts sprite 885 at
 * frame 52. The child's five sound tags are frames 23,47,74,100,126, measured
 * from its playing label at frame 11. Preserve the tags, not guessed delays. */
static inline unsigned lume_gramophone_sound(LumeChildClock *c,unsigned frame) {
  static const unsigned at[]={12,36,63,89,115},id[]={10,9,8,7,9};
  if(c->event>=5 || frame==~0u || frame<at[c->event])return 0;
  return id[c->event++];
}
#endif
