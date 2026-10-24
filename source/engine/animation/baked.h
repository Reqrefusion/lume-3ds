#ifndef LE_BAKED_ANIMATION_H
#define LE_BAKED_ANIMATION_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
/* Source-relative frames and partial property updates, independent of game VM. */
typedef struct {uint32_t clip,frame,pointer,next,first,count;} LeBakedFrame;
typedef struct {uint32_t slot,mask;float value[15];} LeBakedMove;
typedef struct {void *bytes;const LeBakedFrame *frames;const LeBakedMove *moves;unsigned frame_count,move_count;} LeBakedAnimation;
/* Initialize as {0}; load replaces owned data and failure leaves it empty. */
uint32_t le_baked_fingerprint(const void*,size_t);
bool le_baked_load(LeBakedAnimation*,const char*,const void*,size_t,unsigned);
void le_baked_free(LeBakedAnimation*);
#endif
