#ifndef GAME_RENDER_H
#define GAME_RENDER_H
#include "game.h"
#include "render.h"
void lume_game_render(uint8_t *rgb, const uint8_t *background,
                      const LumeGame *g, int menu);
#endif
