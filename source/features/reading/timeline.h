#ifndef LUME_READING_TIMELINE_H
#define LUME_READING_TIMELINE_H
#include <stdint.h>
/* Inclusive source-frame intervals; gaps contain no dialogue. A dropped
   display frame selects the current cue without extending the film clock. */
typedef struct {unsigned first,last;char label[64];} LumeTimedReadingCue;
static inline int lume_read_cue_at(const LumeTimedReadingCue *c,unsigned count,unsigned frame){
 for(unsigned i=0;i<count;i++)if(frame>=c[i].first&&frame<=c[i].last)return (int)i;
 return -1;
}
static inline unsigned lume_read_cue_page(const LumeTimedReadingCue *cue,unsigned frame,unsigned pages){
 if(pages<2||frame<=cue->first)return 0;
 if(frame>=cue->last)return pages-1;
 return (unsigned)(((uint64_t)frame-cue->first)*pages/((uint64_t)cue->last-cue->first+1));
}
#endif
