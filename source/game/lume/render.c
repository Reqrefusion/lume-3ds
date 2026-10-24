#ifndef LUME_MEDIA_TOP
#define LUME_MEDIA_TOP 13
#define LUME_MEDIA_BOTTOM 226
#endif
#include "render.h"
#include "key_points.h"
#include "fallback_font.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
const char *lume_asset_names[LUME_PUZZLE_COUNT] = {
    "wires", "piano", "solar", "keys", "cupboard", "map", "door"};
static const char *titles[LUME_PUZZLE_COUNT] = {
    "WIRE",        "PIANO", "SOLAR PANEL", "KEYS",
    "CUPBOARD LOCK", "MAP", "DOOR LOCK"};
static const float points[12][2] = {
    {649.7f, 328.45f},  {649.7f, 425.1f},  {306.45f, 324.15f},
    {311.4f, 437.1f},   {657.4f, 108.75f}, {433.55f, 109.65f},
    {308.75f, 108.75f}, {543.3f, 109.65f}, {307.35f, 208.25f},
    {535.3f, 430.1f},   {648.1f, 204.7f},  {429.25f, 433.1f}};
static const float key_x[8] = {203.15f, 246.15f, 290.15f, 334.15f,
                               379.2f,  424.15f, 467.2f,  510.2f};
static const int cabinet_x[3] = {136, 162, 185}, cabinet_y[3] = {129, 129, 129};
static const int door_x[3] = {117, 162, 205}, door_y[3] = {92, 67, 89};
static const int cabinet_up[3][2] = {{136, 105}, {162, 105}, {185, 105}},
                 cabinet_down[3][2] = {{136, 158}, {162, 158}, {185, 158}};
static const int door_up[3][2] = {{125, 81}, {176, 68}, {212, 101}},
                 door_down[3][2] = {{110, 116}, {138, 73}, {186, 73}};
static uint8_t *frame;
static void pixel(int x, int y, uint32_t c) {
  if ((unsigned)x < 320 && (unsigned)y < 240) {
    int i = (y * 320 + x) * 3;
    frame[i] = c >> 16;
    frame[i + 1] = c >> 8;
    frame[i + 2] = c;
  }
}
static void alpha_pixel(int x, int y, uint32_t c, unsigned alpha) {
  if ((unsigned)x >= 320 || (unsigned)y >= 240 || !alpha)
    return;
  int i = (y * 320 + x) * 3;
  unsigned r = c >> 16 & 255, g = c >> 8 & 255, b = c & 255;
  frame[i] = (r * alpha + frame[i] * (255 - alpha)) / 255;
  frame[i + 1] = (g * alpha + frame[i + 1] * (255 - alpha)) / 255;
  frame[i + 2] = (b * alpha + frame[i + 2] * (255 - alpha)) / 255;
}
static void rect(int x, int y, int w, int h, uint32_t c) {
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++)
      pixel(xx, yy, c);
}
static void text(int x, int y, const char *s, uint32_t c, int scale) {
  for (; *s; s++, x += 6 * scale) {
    unsigned ch = (unsigned char)*s;
    if (ch < 32 || ch > 126)
      ch = '?';
    for (int yy = 0; yy < 10; yy++)
      for (int xx = 0; xx < 6; xx++)
        for (int dy = 0; dy < scale; dy++)
          for (int dx = 0; dx < scale; dx++)
            alpha_pixel(x + xx * scale + dx, y + yy * scale + dy, c,
                        lume_font_alpha[ch - 32][yy * 6 + xx]);
  }
}
void lume_draw_text(uint8_t *rgb, int x, int y, const char *s, uint32_t c,
                    int scale) {
  frame = rgb;
  text(x, y, s, c, scale);
}

static void line(int x, int y, int ex, int ey, uint32_t c) {
  int dx = abs(ex - x), sx = x < ex ? 1 : -1, dy = -abs(ey - y),
      sy = y < ey ? 1 : -1, err = dx + dy;
  for (;;) {
    pixel(x, y, c);
    if (x == ex && y == ey)
      break;
    int e = 2 * err;
    if (e >= dy) {
      err += dy;
      x += sx;
    }
    if (e <= dx) {
      err += dx;
      y += sy;
    }
  }
}
static void map_point(float gx, float gy, int *x, int *y) {
  float px = 56.85f + .75f * gx, py = 7.6f + .75f * gy;
  *x = (int)((.989837646f * px + .095306396f * py + 9.25f) * .4f);
  *y = (int)((-.095321655f * px + .989837646f * py + 113.55f) * .4f + 13);
}
/* Atlas is sampled offline from the original RGBA bitmap, with premultiplied
   alpha filtering for each cell size and rotation. Keep the board texture and
   thin traces; avoid float conversions in the ARM11 pixel loop. */
static void solar_tiles(const uint8_t *tiles, const SolarPuzzle *p,
                        int level, int ox, int oy) {
  int side = 19 * level;
  size_t base = 4 + (level == 2 ? 64u * 4 * 19 * 19 * 4 : 0);
  for (int row = 0; row < 8; row++)
    for (int col = 0; col < 8; col++) {
      int cell = row * 8 + col;
      const uint8_t *tile = tiles + base +
          ((size_t)cell * 4 + (p->turns[cell] & 3)) * side * side * 4;
      int x0 = ox + level * (col * 149 / 8),
          x1 = ox + level * ((col + 1) * 149 / 8);
      int y0 = oy + level * (row * 149 / 8),
          y1 = oy + level * ((row + 1) * 149 / 8);
      for (int y = y0; y < y1; y++) {
        if (y < 13 || y >= 227)
          continue;
        for (int x = x0; x < x1; x++) {
          if ((unsigned)x >= 320)
            continue;
          const uint8_t *c = tile + ((y - y0) * side + x - x0) * 4;
          alpha_pixel(x, y, ((uint32_t)c[0] << 16) |
                      ((uint32_t)c[1] << 8) | c[2], c[3]);
        }
      }
    }
}
void lume_render_solar_zoom(uint8_t *rgb, const SceneAssets *assets,
                            const SolarPuzzle *p, int cx, int cy) {
  if (!assets->solar_tiles)
    return;
  frame = rgb;
  solar_tiles(assets->solar_tiles, p, 2, 2 * (49 - cx + 80),
              13 + 2 * (44 - cy + 53));
}
/* The map's red selection is sprite gridSquare/highlighted, not a circle
   primitive. Two prefiltered sizes avoid trigonometry and magnified jaggies
   on ARM11 while retaining the original alpha and uneven line weight. */
static void map_ink_stamp(const SceneAssets *a, const MapPuzzle *p,
                          int cx, int cy, bool detail) {
  if (p->x < 0 || p->x >= 10 || p->y < 0 || p->y >= 10 || !a->map_ink ||
      memcmp(a->map_ink, "INK2", 4)) return;
  int x,y;
  map_point(p->x * 40 + 20, p->y * 40 + 20, &x, &y);
  int side = detail ? 64 : 32;
  if (detail) { x = (x - cx) * 2 + 160; y = (y - cy) * 2 + 119; }
  const uint8_t *ink = a->map_ink + 4 +
      (p->ink_variant & 3u) * LUME_MAP_INK_VARIANT_BYTES +
      (detail ? 32 * 32 * 4 : 0);
  for (int yy = 0; yy < side; yy++)
    for (int xx = 0; xx < side; xx++) {
      const uint8_t *p = ink + (yy * side + xx) * 4;
      alpha_pixel(x + xx - side / 2, y + yy - side / 2,
                  (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2], p[3]);
    }
}
void lume_render_map_zoom(uint8_t *rgb, const SceneAssets *a,
                           const MapPuzzle *p, int cx, int cy) {
  frame = rgb;
  map_ink_stamp(a, p, cx, cy, true);
}
/* AVM1's loose wire follows the mouse with a 12-unit round stroke and the
 * same fixed quadratic control point as completed wires. Coverage is merged
 * before blending, so tessellation joins cannot darken the red stroke. */
void lume_render_wire_preview(uint8_t *rgb,const WirePuzzle *p) {
  if(p->selected<0 || p->selected>=12 || p->preview_x<0 || p->preview_x>=320 || p->preview_y<13 || p->preview_y>=227)return;
  static uint8_t mask[320*240];memset(mask,0,sizeof mask);
  float ax=points[p->selected][0]*.4f,ay=points[p->selected][1]*.4f+13;
  float px=ax,py=ay;int minx=320,miny=240,maxx=0,maxy=0;
  for(int k=1;k<=64;k++) {
    float t=k/64.f,s=1-t;
    float qx=s*s*ax+2*s*t*80+t*t*p->preview_x,qy=s*s*ay+2*s*t*173+t*t*p->preview_y;
    float dx=qx-px,dy=qy-py,len=dx*dx+dy*dy,inv=len>0?1/len:0;
    int x0=(int)floorf(fminf(px,qx)-3),x1=(int)ceilf(fmaxf(px,qx)+3);
    int y0=(int)floorf(fminf(py,qy)-3),y1=(int)ceilf(fmaxf(py,qy)+3);
    if(x0<0)x0=0;
    if(x1>319)x1=319;
    if(y0<13)y0=13;
    if(y1>226)y1=226;
    if(x0<minx)minx=x0;
    if(x1>maxx)maxx=x1;
    if(y0<miny)miny=y0;
    if(y1>maxy)maxy=y1;
    for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++) {
      float u=((x+.5f-px)*dx+(y+.5f-py)*dy)*inv;if(u<0)u=0;if(u>1)u=1;
      float ex=x+.5f-px-u*dx,ey=y+.5f-py-u*dy,d2=ex*ex+ey*ey;
      unsigned coverage=d2<=1.9f*1.9f?255:d2>=2.9f*2.9f?0:(unsigned)((2.9f-sqrtf(d2))*255+.5f);
      unsigned off=y*320+x;if(coverage>mask[off])mask[off]=coverage;
    }
    px=qx;py=qy;
  }
  frame=rgb;
  for(int y=miny;y<=maxy;y++)for(int x=minx;x<=maxx;x++)if(mask[y*320+x])alpha_pixel(x,y,0x990000,mask[y*320+x]);
}
void lume_render(uint8_t *rgb, const SceneAssets *assets, const LumePuzzles *p,
                 int mode, int status) {
  frame = rgb;
  if (mode < 0 || mode >= LUME_PUZZLE_COUNT)
    return;
  if (assets->background[mode])
    memcpy(frame, assets->background[mode], LUME_FRAME_BYTES);
  else
    memset(frame, 0, LUME_FRAME_BYTES);
  if (mode == LUME_WIRES) {
    /* Source connector states and AVM1 wire strokes are composited by the
       scene owner. No invented red polylines or yellow selection circles. */
  } else if (mode == LUME_PIANO) {
    /* The original piano does not print a numeric note history or a yellow
       debug bar. Notes remain audible; the puzzle state is unchanged. */
  } else if (mode == LUME_SOLAR && assets->solar_tiles) {
    solar_tiles(assets->solar_tiles, &p->solar, 1, 49, 44);
  } else if (mode == LUME_SOLAR && assets->pattern) {
    for (int row = 0; row < 8; row++)
      for (int col = 0; col < 8; col++) {
        int x0 = 49 + col * 149 / 8, x1 = 49 + (col + 1) * 149 / 8,
            y0 = 44 + row * 149 / 8, y1 = 44 + (row + 1) * 149 / 8;
        int rotation = p->solar.turns[row * 8 + col];
        for (int y = y0; y < y1; y++)
          for (int x = x0; x < x1; x++) {
            int tx = (x - x0) * 20 / (x1 - x0), ty = (y - y0) * 20 / (y1 - y0),
                sx = tx, sy = ty;
            if (rotation == 1) {
              sx = ty;
              sy = 19 - tx;
            } else if (rotation == 2) {
              sx = 19 - tx;
              sy = 19 - ty;
            } else if (rotation == 3) {
              sx = 19 - ty;
              sy = tx;
            }
            int off = ((row * 20 + sy) * 160 + col * 20 + sx) * 3;
            uint32_t c = (assets->pattern[off] << 16) |
                         (assets->pattern[off + 1] << 8) |
                         assets->pattern[off + 2];
            pixel(x, y, c);
          }
      }
    for (int i = 0; i <= 8; i++) {
      line(49 + i * 149 / 8, 44, 49 + i * 149 / 8, 193, 0x465044);
      line(49, 44 + i * 149 / 8, 198, 44 + i * 149 / 8, 0x465044);
    }
  } else if (mode == LUME_KEYS) {
    /* Source selected key children retain their shape and paper texture. */
  } else if (mode == LUME_CUPBOARD || mode == LUME_DOOR) {
    const LockPuzzle *lock = mode == LUME_CUPBOARD ? &p->cupboard : &p->door;
    const int *xs = mode == LUME_CUPBOARD ? cabinet_x : door_x,
              *ys = mode == LUME_CUPBOARD ? cabinet_y : door_y;
    for (int i = 0; !assets->source_lock_art && i < 3; i++) {
      const uint8_t *atlas =
          mode == LUME_DOOR ? assets->door_digits : assets->cupboard_digits;
      if (atlas) {
        const uint8_t *glyph = atlas + (i * 10 + lock->digits[i]) * 1024;
        for (int yy = 0; yy < 32; yy++)
          for (int xx = 0; xx < 32; xx++)
            alpha_pixel(xs[i] - 16 + xx, ys[i] - 16 + yy, 0x23241b,
                        glyph[yy * 32 + xx]);
      } else {
        char s[2] = {(char)('0' + lock->digits[i]), 0};
        text(xs[i] - 5, ys[i] - 7, s, 0x193c39, 1);
      }
      char s[2] = {0, 0};
      if (mode == LUME_CUPBOARD) {
        s[0] = '0' + (lock->digits[i] + 9) % 10;
        text(xs[i] - 2, 111, s, 0x334333, 1);
        s[0] = '0' + (lock->digits[i] + 1) % 10;
        text(xs[i] - 2, 141, s, 0x334333, 1);
      }
    }
    /* Authored lock progress is composited by the scene owner before zoom;
       preserve its original lamp shapes instead of inventing green circles. */
  } else if (mode == LUME_MAP) {
    /* The photograph already contains the fine printed paper grid. */
    map_ink_stamp(assets, &p->map, 0, 0, false);
  }
  if (status == -9999) {
    rect(0, 0, 320, 12, 0x102027);
    text(5, 3, "<", 0xffdc91, 1);
    text(25, 3, titles[mode], 0xf6e8c9, 1);
    char count[16];
    int solved = 0;
    for (int i = 0; i < LUME_PUZZLE_COUNT; i++)
      solved += p->completed[i];
    snprintf(count, sizeof(count), "%d/7  %d/7", mode + 1, solved);
    text(242, 3, count, 0xf6e8c9, 1);
    text(308, 3, ">", 0xffdc91, 1);
    rect(0, 227, 320, 13, 0x102027);
    text(8, 230, "RESET", 0xffdc91, 1);
    text(276, 230, "TRY", 0xffdc91, 1);
    const char *state =
        status == -2
            ? "NO VIDEO"
            : (p->completed[mode]
                   ? "SOLVED"
                   : (status > 0 ? "CORRECT"
                                 : (status < 0 ? "INCORRECT" : "L/R PUZZLE")));
    text(119, 230, state, p->completed[mode] ? 0x99e4ad : 0xf6e8c9, 1);
  }
}
int lume_touch(LumePuzzles *p, int mode, int x, int y) {
  if (x < 0 || x >= 320 || y < 13 || y >= 227)
    return -1;
  float ox = x / .4f, oy = (y - 13) / .4f;
  if (mode == LUME_WIRES) {
    if (x >= 38 && x <= 88 && y >= 182 && y <= 218)
      return y < 199 ? 200 : 100;
    for (int i = 0; i < 12; i++) {
      float dx = ox - points[i][0], dy = oy - points[i][1];
      if (dx * dx + dy * dy < 30 * 30) {
        wire_touch(&p->wire, i + 1);
        return 0;
      }
    }
  } else if (mode == LUME_PIANO) {
    if (oy >= 190 && oy <= 335 && ox >= 190 && ox <= 555) {
      int key = -1;
      float best = 1000;
      for (int i = 0; i < 8; i++) {
        float d = fabsf(ox - key_x[i] - 20);
        if (d < best) {
          best = d;
          key = i;
        }
      }
      /* The source still sounds/animates later presses after a wrong phrase.
         Keep the first five puzzle notes until clear, but return every key. */
      if (key >= 0 && best < 28) {
        piano_note(&p->piano, key + 1);
        return key + 1;
      }
    }
    if (x > 141 && x < 167 && y > 162 && y < 193)
      return 200;
  } else if (mode == LUME_SOLAR) {
    if (x >= 49 && x < 198 && y >= 44 && y < 193) {
      solar_turn(&p->solar, (((y - 44 + 1) * 8 - 1) / 149) * 8 +
                           ((x - 49 + 1) * 8 - 1) / 149);
      return 0;
    }
  } else if (mode == LUME_KEYS) {
    int key = -1;
    float best = 35 * 35;
    for (int i = 0; i < 14; i++) {
      float dx = ox - lume_key_points[i][0], dy = oy - lume_key_points[i][1],
            d = dx * dx + dy * dy;
      if (d < best) {
        best = d;
        key = i;
      }
    }
    if (key >= 0) {
      keys_toggle(&p->keys, key);
      return 0;
    }
    if (x >= 219 && x <= 257 && y >= 173 && y <= 207)
      return 100;
  } else if (mode == LUME_CUPBOARD || mode == LUME_DOOR) {
    LockPuzzle *lock = mode == LUME_CUPBOARD ? &p->cupboard : &p->door;
    const int *xs = mode == LUME_CUPBOARD ? cabinet_x : door_x,
              *ys = mode == LUME_CUPBOARD ? cabinet_y : door_y;
    const int (*ups)[2] = mode == LUME_CUPBOARD ? cabinet_up : door_up,
              (*downs)[2] = mode == LUME_CUPBOARD ? cabinet_down : door_down;
    int digit = -1, delta = 0, best = 11 * 11;
    for (int i = 0; i < 3; i++) {
      int dx = x - ups[i][0], dy = y - ups[i][1], d = dx * dx + dy * dy;
      if (d < best) {
        best = d;
        digit = i;
        delta = 1;
      }
      dx = x - downs[i][0];
      dy = y - downs[i][1];
      d = dx * dx + dy * dy;
      if (d < best) {
        best = d;
        digit = i;
        delta = -1;
      }
    }
    if (digit >= 0) {
      lock_adjust(lock, digit, delta);
      return 0;
    }
    for (int i = 0; i < 3; i++)
      if (abs(x - xs[i]) < 10 && abs(y - ys[i]) < 10) {
        lock_adjust(lock, i, y < ys[i] ? 1 : -1);
        return 0;
      }
    if (abs(x - 160) < 17 && abs(y - (mode == LUME_CUPBOARD ? 183 : 177)) < 17)
      return 100;
  } else if (mode == LUME_MAP) {
    float px = ox - 9.25f, py = oy - 113.55f,
          det = .989837646f * .989837646f + .095306396f * .095321655f;
    float lx = (.989837646f * px - .095306396f * py) / det,
          ly = (.095321655f * px + .989837646f * py) / det;
    float gx = (lx - 56.85f) / .75f, gy = (ly - 7.6f) / .75f;
    if (gx >= 0 && gx < 400 && gy >= 0 && gy < 400) {
      map_select(&p->map, gx / 40, gy / 40);
      return 0;
    }
    if (x >= 209 && x <= 272 && y >= 179 && y <= 214)
      return 100;
  }
  return -1;
}
void lume_zoom_center(int mode, int *x, int *y) {
  *x = 160;
  *y = 120;
  if (mode == LUME_SOLAR) {
    *x = 123;
    *y = 119;
  }
  if (mode == LUME_KEYS) {
    *x = 120;
    *y = 122;
  }
  if (mode == LUME_MAP) {
    *x = 94;
    *y = 112;
  }
}
void lume_zoom_touch(int cx, int cy, int *x, int *y) {
  *x = cx - 80 + *x / 2;
  *y = cy - 53 + (*y - 13) / 2;
}
void lume_zoom_frame(uint8_t *rgb, uint8_t *scratch, int cx, int cy) {
  memcpy(scratch, rgb, LUME_FRAME_BYTES);
  for (int y = 13; y < 227; y++)
    for (int x = 0; x < 320; x++) {
      int sx = x, sy = y;
      lume_zoom_touch(cx, cy, &sx, &sy);
      memcpy(rgb + (y * 320 + x) * 3, scratch + (sy * 320 + sx) * 3, 3);
    }
  frame = rgb;
  text(212, 3, "2X", 0xffdc91, 1);
}
