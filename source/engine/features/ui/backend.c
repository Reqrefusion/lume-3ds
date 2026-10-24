#include "engine/features/ui/backend.h"
#include "engine/core/jobs_state.h"
#include "engine/features/locale/locale.h"
#include <string.h>
LumeLocale lume_locale={.music=100,.effects=100,.stereo_percent=100};
static const uint8_t (*fallback_font)[60];
static LumeUiTextFallback fallback_draw;
static char user_directory[160]=".";
void lume_ui_set_fallback(const uint8_t (*font)[60],LumeUiTextFallback draw){fallback_font=font;fallback_draw=draw;}
unsigned lume_ui_fallback_alpha(unsigned ch,unsigned pixel) {
 if(!fallback_font||pixel>=60)return 0;
 if(ch<32||ch>=127)ch='?';
 return fallback_font[ch-32][pixel];
}
void lume_ui_fallback_text(uint8_t *rgb,int x,int y,const char *s,uint32_t color,int scale){if(fallback_draw)fallback_draw(rgb,x,y,s,color,scale);}
void lume_ui_set_user_directory(const char *directory){snprintf(user_directory,sizeof user_directory,"%s",directory&&*directory?directory:".");}
bool lume_ui_user_path(char *out,size_t bytes,const char *relative) {
 if(!relative||strstr(relative,"..")||strchr(relative,':')||relative[0]=='/'||strchr(relative,'\\'))return false;
 int n=snprintf(out,bytes,"%s/%s",user_directory,relative);return n>=0&&(size_t)n<bytes;
}
FILE *lume_ui_asset_open(const char *relative) {
 char path[300];return lume_runtime_asset_path(path,sizeof path,relative)?fopen(path,"rb"):NULL;
}
