#ifndef LUME_RENDER_H
#define LUME_RENDER_H
#include "puzzles.h"
#define LUME_FRAME_BYTES (320 * 240 * 3)
#define LUME_MAP_INK_VARIANT_BYTES ((32 * 32 + 64 * 64) * 4)
#define LUME_MAP_INK_BYTES (4 + 4 * LUME_MAP_INK_VARIANT_BYTES)
#define LUME_PATTERN_BYTES (160 * 160 * 3)
#define LUME_SOLAR_TILE_BYTES (4 + 64 * 4 * (19 * 19 + 38 * 38) * 4)
typedef struct {
  const uint8_t *background[LUME_PUZZLE_COUNT];
  const uint8_t *pattern;
  const uint8_t *solar_tiles;
  const uint8_t *map_ink;
  const uint8_t *door_digits, *cupboard_digits;
  bool source_lock_art;
} SceneAssets;
void lume_draw_text(uint8_t *rgb, int x, int y, const char *s, uint32_t color,
                    int scale);
extern const char *lume_asset_names[LUME_PUZZLE_COUNT];
void lume_render(uint8_t *rgb, const SceneAssets *assets, const LumePuzzles *p,
                 int mode, int status);
/* Touch result: 1..8 piano sample; 100 submit; 0 ordinary change; -1 no hit. */
int lume_touch(LumePuzzles *p, int mode, int x, int y);
void lume_render_solar_zoom(uint8_t *rgb, const SceneAssets *assets,
                            const SolarPuzzle *p, int cx, int cy);
void lume_render_map_zoom(uint8_t *rgb, const SceneAssets *assets,
                           const MapPuzzle *p, int cx, int cy);
void lume_render_wire_preview(uint8_t *rgb,const WirePuzzle *p);
void lume_zoom_center(int mode, int *x, int *y);
void lume_zoom_frame(uint8_t *rgb, uint8_t *scratch, int cx, int cy);
void lume_zoom_touch(int cx, int cy, int *x, int *y);
#endif
