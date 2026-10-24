#ifndef LUME_UI_BACKEND_H
#define LUME_UI_BACKEND_H
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
typedef void (*LumeUiTextFallback)(uint8_t*,int,int,const char*,uint32_t,int);
void lume_ui_set_fallback(const uint8_t (*font)[60],LumeUiTextFallback draw);
unsigned lume_ui_fallback_alpha(unsigned ch,unsigned pixel);
void lume_ui_fallback_text(uint8_t*,int,int,const char*,uint32_t,int);
void lume_ui_set_user_directory(const char *directory);
bool lume_ui_user_path(char *out,size_t bytes,const char *relative);
FILE *lume_ui_asset_open(const char *relative);
#endif
