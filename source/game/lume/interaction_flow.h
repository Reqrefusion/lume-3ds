#ifndef LUME_INTERACTION_FLOW_H
#define LUME_INTERACTION_FLOW_H
#include "exterior_motion.h"

/* Scene-bound transaction: commit only after animation opens successfully.
   Completion consumes the action once; cancellation never mutates inventory. */
typedef struct {
  int position, destination, action, scene;
  bool active;
} LumeInteractionFlow;
typedef enum { LUME_MOVE_DONE, LUME_MOVE_CANCELLED, LUME_MOVE_TRY_DOOR,
               LUME_MOVE_FRONT_RETURN, LUME_MOVE_STAIRS } LumeMoveResult;
static inline void lume_interaction_reset(LumeInteractionFlow *f, int scene) {
  *f=(LumeInteractionFlow){.position=lume_motion_initial(scene),
    .destination=lume_motion_initial(scene),.action=-1,.scene=scene};
}
static inline void lume_interaction_cancel(LumeInteractionFlow *f) {
  f->active=false;f->action=-1;
}
/* Native fence policy: far click speaks; explicit near click commits pickup.
   AS988/frame132 supplies the click branches, but AS806/frame32 also takes
   during the original walk. We intentionally do not replay that side effect.
   See docs/ActionScript_Baglantilari.md for the timing adaptation. */
static inline const char *lume_interaction_fence_line(const LumeGame *g,int position) {
  if((g->inventory&(1u<<BLADES))||g->turbine>=LUME_TURBINE_BLADES)return "puzzleComplete";
  return position==1?NULL:"bladesThought";
}
static inline int lume_interaction_destination(const LumeGame *g,int position,int action) {
  if(action==TAKE_BLADES)return position; /* Never queue a future pickup. */
  return lume_motion_action_position(g,position,action);
}
static inline int lume_interaction_floor(int scene,int x) {
  switch(scene) {
    case ROOF:return x>=225;
    case FRONT:return x<130;
    case HALL:return x>=110;
    default:return -1;
  }
}
static inline void lume_interaction_begin(LumeInteractionFlow *f,int scene,int destination,int action) {
  f->scene=scene;f->destination=destination;f->action=action;f->active=true;
}
static inline LumeMoveResult lume_interaction_finish(LumeInteractionFlow *f,int scene,
                                                     bool retained,int *action) {
  *action=-1;
  if(!f->active)return LUME_MOVE_CANCELLED;
  int queued=f->action;
  lume_interaction_cancel(f);
  if(!retained||scene!=f->scene)return LUME_MOVE_CANCELLED;
  f->position=f->destination;
  if(scene==FRONT&&queued==OPEN_FRONT&&f->destination==2)return LUME_MOVE_TRY_DOOR;
  *action=queued;
  if(scene==FRONT&&queued==OPEN_FRONT&&f->destination==3) {
    f->position=f->destination=0;return LUME_MOVE_FRONT_RETURN;
  }
  if(scene==HALL&&queued==GO_STAIRS&&f->destination==2) {
    f->position=f->destination=1;return LUME_MOVE_STAIRS;
  }
  return LUME_MOVE_DONE;
}
#endif
