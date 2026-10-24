#ifndef LUME_PROFILE_H
#define LUME_PROFILE_H
#include <3ds.h>
void lume_profile_frame(u64 start, u64 audio, u64 draw, u64 present, u64 end,
                        int scene, int clip, bool shown);
#endif
