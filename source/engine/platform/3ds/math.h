#ifndef LUME_PLATFORM_MATH_H
#define LUME_PLATFORM_MATH_H
#include <stdbool.h>
static inline bool vfp_set_fz_dn(void) {
  /* Denormals stall VFP11 unless FZ is set; decay loops can generate them.
     FPSCR is per-thread, so every application float worker calls this at entry.
   */
  unsigned fpscr;
  __asm__ volatile("vmrs %0, fpscr" : "=r"(fpscr));
  fpscr |= (1u << 24) | (1u << 25);
  __asm__ volatile("vmsr fpscr, %0" ::"r"(fpscr));
  __asm__ volatile("vmrs %0, fpscr" : "=r"(fpscr));
  return (fpscr & ((1u << 24) | (1u << 25))) == ((1u << 24) | (1u << 25));
}
#endif
