#ifndef LUME_PUZZLE_FLOW_H
#define LUME_PUZZLE_FLOW_H
#include "puzzles.h"
#include <stdint.h>
#include <stddef.h>
/* resetTimerAnim plays source frames 11..62 at 25 Hz. Solar's result is
   child sprite 309 and hands back at frame 115. Other locks change popup
   directly; they do not use a fabricated universal success screen. */
typedef struct { int mode; uint64_t started; unsigned ticks, shown; bool power_preview; } LumePuzzleFlow;
static inline void lume_puzzle_flow_begin(LumePuzzleFlow *f,int mode,uint64_t now) {
 f->mode=mode;f->started=now;f->shown=~0u;f->power_preview=false;
 f->ticks=mode==LUME_SOLAR?115:mode==LUME_CUPBOARD||mode==LUME_DOOR?1:52;
}
static inline unsigned lume_puzzle_flow_frame(const LumePuzzleFlow *f,uint64_t now) {
 uint64_t tick=now<f->started?0:(now-f->started)/40;return tick>=f->ticks?f->ticks-1:(unsigned)tick;
}
static inline bool lume_puzzle_flow_ready(const LumePuzzleFlow *f,uint64_t now) {
 return f->mode>=0 && now>=f->started && f->shown==f->ticks-1 && now-f->started>=(uint64_t)f->ticks*40;
}
static inline const char *lume_puzzle_exit_line(int mode) {
 static const char *lines[]={"wiresPuzzleComplete","broomcupboardUnlocked","rotationPuzzleComplete","keyPuzzleComplete","cupboardPuzzleComplete","mapPuzzleComplete",NULL};
 return (unsigned)mode<LUME_PUZZLE_COUNT?lines[mode]:NULL;
}
#endif
