#ifndef LUME_PROPS_H
#define LUME_PROPS_H
#include <stdbool.h>
#include <stdint.h>
bool lume_props_init(void);
void lume_props_quit(void);
void lume_props_set_visibility(bool turbine,bool solar);
bool lume_props_world(uint8_t *rgb,unsigned scene,int turbine,bool blades,bool turbine_visible,bool solar_visible,int eye);
bool lume_props_open(const char *name, unsigned frames);
void lume_props_close(void);
void lume_props_frame(uint8_t *rgb, unsigned frame, int turbine, bool blades,
                      int eye);
#endif
