#ifndef LUME_PLATFORM_CLOCK_H
#define LUME_PLATFORM_CLOCK_H
#include <stdint.h>
#ifdef __3DS__
#include <3ds.h>
#elif defined(LUME_HOST_JOBS)
#include <time.h>
#endif
static inline uint64_t lume_platform_ticks(void) {
#ifdef __3DS__
 return svcGetSystemTick();
#elif defined(LUME_HOST_JOBS)
 struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;
#else
 return 0;
#endif
}
#endif
