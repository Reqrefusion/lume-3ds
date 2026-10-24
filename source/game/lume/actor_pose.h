#ifndef LUME_ACTOR_POSE_H
#define LUME_ACTOR_POSE_H
#include <stdint.h>
#include <stdbool.h>
extern const int lume_actor_feet[7][2];
extern const float lume_actor_heights[7];
void lume_actor_init(void);
/* Restore the camera's authored default pose after leaving another waypoint. */
void lume_actor_restore(int scene);
bool lume_actor_idle(uint8_t*,int,int,int,int,int);
/* Ground contact follows the retained a/b landing crop, not the scene's
   default waypoint (which can differ after an authored walk). */
bool lume_actor_ground(int scene,int x,int y,int *foot_x,int *foot_y);
void lume_actor_capture(int,int,int,unsigned,unsigned,const uint8_t*);
/* Native pose identity follows the same restore/capture events as its overview. */
const char *lume_actor_detail(int scene);
void lume_actor_capture_named(int,int,int,unsigned,unsigned,const uint8_t*,const char*);
/* Reports an unretainable terminal crop instead of silently using an old pose. */
bool lume_actor_capture_named_checked(int,int,int,unsigned,unsigned,const uint8_t*,const char*);
#endif
