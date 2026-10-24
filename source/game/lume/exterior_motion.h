#ifndef LUME_EXTERIOR_MOTION_H
#define LUME_EXTERIOR_MOTION_H
#include "game.h"
#include <stddef.h>
/* Evidence: sprite 988 frames 132/400/701; see docs/ActionScript_Baglantilari.md.
 * Each AVM1 camera owns its position labels and savedMove/savedPopup queue.
 * Hall enters at b; other cameras enter at a. Closed-up pantry has no actor.
 * A click never synthesizes a floor slope or a replacement walk cycle. */
static inline int lume_motion_initial(int scene) { return scene == HALL ? 1 : 0; }
static inline const char *lume_motion_clip(int scene, int from, int to) {
  if (from == to) return NULL;
  if (scene == ROOF) return to ? "walk_roof_ab" : "walk_roof_ba";
  if (scene == FRONT) {
    if (to == 2) return from ? "walk_front_bd" : "walk_front_ad";
    return to ? "walk_front_ab" : "walk_front_ba";
  }
  if (scene == HALL) {
    if (to == 2) return "walk_hall_as";
    return to ? "walk_hall_ab" : "walk_hall_ba";
  }
  return NULL; /* Other cameras expose fixed interactions/authored navigation. */
}
static inline int lume_motion_action_position(const LumeGame *g, int current, int action) {
  int scene=g->scene;
  if (scene == ROOF) {
    /* Fence pickup is handled in place by its own source onRelease. */
    if (action == TAKE_BLADES) return current;
    /* A locked door responds in place, before any movement is scheduled. */
    if (action == GO_TOP_DOOR && !g->top_door) return current;
    if (action == GO_GARDEN || action == GO_LADDER || action == GO_TOP_DOOR) return 0;
  } else if (scene == FRONT) {
    if (action == OPEN_SOLAR) return 1;
    if (action == OPEN_FRONT) return 2;
    if (action == READ_LETTER || action == GO_LADDER) return 0;
  } else if (scene == HALL) {
    if (action == OPEN_KEYS || action == OPEN_CUPBOARD || action == TAKE_TOOLS || action == TAKE_WIRE) return 0;
    if (action == GO_POWER || action == GO_LADDER || action == OPEN_PANTRY) return 1;
    /* This label is named b_to_stairs, but camera4's AS starts it from a. */
    if (action == GO_STAIRS) return current == 0 ? 2 : 1;
  }
  return current;
}
#endif
