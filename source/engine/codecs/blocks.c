#include "engine/codecs/blocks.h"
/* @hotpath A block has two RGB565 endpoints and sixteen two-bit indices.
   Only fixed integer arithmetic is used; alpha/UI layers are separate. */
void lume_decode_blocks(const uint8_t *src, uint8_t *rgb) {
  for (int by = 0; by < 240; by += 4)
    for (int bx = 0; bx < 320; bx += 4, src += 8) {
      unsigned a = src[0] | src[1] << 8, b = src[2] | src[3] << 8;
      uint8_t p[4][3];
      unsigned r = a >> 11, g = (a >> 5) & 63, bl = a & 31;
      p[0][0] = (r << 3) | (r >> 2);
      p[0][1] = (g << 2) | (g >> 4);
      p[0][2] = (bl << 3) | (bl >> 2);
      r = b >> 11;
      g = (b >> 5) & 63;
      bl = b & 31;
      p[1][0] = (r << 3) | (r >> 2);
      p[1][1] = (g << 2) | (g >> 4);
      p[1][2] = (bl << 3) | (bl >> 2);
      for (int k = 0; k < 3; k++) {
        if (a > b) {
          p[2][k] = (2 * p[0][k] + p[1][k]) / 3;
          p[3][k] = (p[0][k] + 2 * p[1][k]) / 3;
        } else {
          p[2][k] = (p[0][k] + p[1][k]) / 2;
          p[3][k] = 0;
        }
      }
      unsigned bits =
          src[4] | src[5] << 8 | src[6] << 16 | (unsigned)src[7] << 24;
      for (int y = 0; y < 4; y++) {
        uint8_t *d = rgb + ((by + y) * 320 + bx) * 3;
        for (int x = 0; x < 4; x++, d += 3, bits >>= 2) {
          const uint8_t *c = p[bits & 3];
          d[0] = c[0];
          d[1] = c[1];
          d[2] = c[2];
        }
      }
    }
}
