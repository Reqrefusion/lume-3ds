#ifndef LUME_GAME_SKIP_POLICY_H
#define LUME_GAME_SKIP_POLICY_H
#include "engine/features/ui/skip.h"
/* Source btnSkip belongs to intro/ending, never ordinary walking clips. */
static inline bool lume_skip_visible(int kind){return kind==2||kind==4;}
static inline bool lume_skip_hit(int kind,int x,int y){return lume_skip_visible(kind)&&lume_skip_rect_hit(x,y);}
#endif
