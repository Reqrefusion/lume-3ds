#ifndef LUME_PUZZLE_HELP_H
#define LUME_PUZZLE_HELP_H
#include "puzzles.h"
#include <stddef.h>
/* Original popup text extents, including the title, plus a 10px source margin.
   Apply the existing 800x533 -> 320x240 stage mapping (33px top inset) once.
   Input must already be mapped back through the bottom viewport. These cards
   do not overlap tiles, map squares, Check/Reset controls or the close button. */
static inline const char *lume_puzzle_help_label(int mode,int x,int y) {
 if(mode==LUME_SOLAR&&x>=252&&x<310&&y>=78&&y<152)return "help_solar";
 if(mode==LUME_MAP&&x>=177&&x<296&&y>=13&&y<166)return "help_map";
 return NULL;
}
#endif
