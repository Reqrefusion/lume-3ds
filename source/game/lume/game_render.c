#ifndef LUME_MEDIA_TOP
#define LUME_MEDIA_TOP 13
#define LUME_MEDIA_BOTTOM 226
#endif
#include "game_render.h"
#include <string.h>
/* Desktop toolbar and action-list menus are replaced by native lower-screen
   overlays. Keep the world image clean for stereo, zoom and movie capture. */
void lume_game_render(uint8_t *rgb,const uint8_t *background,const LumeGame *g,int menu) {
 (void)g;(void)menu;
 if(background)memcpy(rgb,background,LUME_FRAME_BYTES);else memset(rgb,0,LUME_FRAME_BYTES);
}
