#ifndef LUME_CLUE_VIEWS_H
#define LUME_CLUE_VIEWS_H
#include "game.h"
#include <stdio.h>
/* Shared names and cache identity prevent a failed read from leaving the
 * previous book on screen. Item visibility is part of a storage view's key. */
static inline const char *lume_clue_asset(const LumeGame *g, char *variant, size_t size) {
  static const char *names[]={"letter","book","gramophone","book2","book3",
                            "photo1","photo2","photo3","meter0","cupboard"};
  if (g->clue<0 || g->clue>=10) return NULL;
  if (g->clue==4) {
    snprintf(variant,size,"book3_%u",g->book_variant%11);
    return variant;
  }
  if (g->clue==9) {
    snprintf(variant,size,"cupboard_m%u",lume_game_cupboard_mask(g));
    return variant;
  }
  if (g->clue==8) return lume_game_power_level(g)==2?"meter2":lume_game_power_level(g)==1?"meter1":"meter0";
  return names[g->clue];
}
static inline int lume_clue_cache_key(const LumeGame *g) {
  if (g->clue<0 || g->clue>=10) return -1;
  int key=g->clue;
  if(g->clue==4)key+=(int)(g->book_variant%11)*16;
  if(g->clue==9)key+=(int)lume_game_cupboard_mask(g)*16;
  if(g->clue==8)key+=(int)lume_game_power_level(g)*256;
  return key;
}
#endif
