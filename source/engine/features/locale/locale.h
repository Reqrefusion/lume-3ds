#ifndef LUME_LOCALIZATION_H
#define LUME_LOCALIZATION_H
#include "engine/render/colour.h"
#include <stdint.h>
#include "engine/features/ui/backend.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#define LUME_LOCALE_GLYPHS 224
#define LUME_LOCALE_LANGS 16
#define LUME_LOCALE_UI 64
typedef struct {char id[16],name[64];} LumeLanguageName;
typedef struct {char key[32],value[160];} LumeTranslation;
typedef struct {
 LumeLanguageName languages[LUME_LOCALE_LANGS];unsigned count,selected,music,effects,stereo_percent,serial;
 uint32_t codes[LUME_LOCALE_GLYPHS];uint8_t font[LUME_LOCALE_GLYPHS*240];unsigned glyphs,ui_count;
 LumeTranslation ui[LUME_LOCALE_UI];bool font_ready;
} LumeLocale;
extern LumeLocale lume_locale;
static inline bool lume_locale_id(const char *s){
 size_t n=strlen(s);if(!n||n>15)return false;
 for(size_t i=0;i<n;i++)if(!((s[i]>='a'&&s[i]<='z')||(s[i]>='0'&&s[i]<='9')||s[i]=='_'))return false;
 return true;
}
static inline FILE *lume_locale_file(const char *name){
 if(!lume_locale.count||!name||strstr(name,"..")||strchr(name,':')||strchr(name,'/'))return NULL;
 char p[300],relative[160];const char *id=lume_locale.languages[lume_locale.selected].id;
 snprintf(relative,sizeof relative,"lang/%s/%s",id,name);
 FILE *f=lume_ui_user_path(p,sizeof p,relative)?fopen(p,"rb"):NULL;
 if(!f)f=lume_ui_asset_open(relative);
 return f;
}
static inline uint32_t lume_utf8(const char **at){
 const unsigned char *p=(const unsigned char*)*at;uint32_t c=*p++;unsigned n=0;
 if(c<128){*at=(const char*)p;return c;}
 if(c>=0xc2&&c<=0xdf){c&=31;n=1;}else if(c>=0xe0&&c<=0xef){c&=15;n=2;}else if(c>=0xf0&&c<=0xf4){c&=7;n=3;}else {(*at)++;return 0xfffd;}
 unsigned k=n;
 while(n--){if((*p&0xc0)!=0x80){(*at)++;return 0xfffd;}c=(c<<6)|(*p++&63);}
 *at=(const char*)p;
 if((k==1&&c<128)||(k==2&&c<2048)||(k==3&&c<65536)||c>0x10ffff||(c>=0xd800&&c<=0xdfff))return 0xfffd;
 return c;
}
static inline unsigned char lume_locale_char(uint32_t c){
 if(c<127)return (unsigned char)c;
 for(unsigned i=95;i<lume_locale.glyphs;i++)if(lume_locale.codes[i]==c)return (unsigned char)(i+32);
 return '?';
}
static inline void lume_locale_encode(char *out,size_t cap,const char *utf8){
 if(!cap)return;
 size_t n=0;while(*utf8&&n+1<cap)out[n++]=(char)lume_locale_char(lume_utf8(&utf8));out[n]=0;
}
static inline const char *lume_tr(const char *key,const char *fallback){
 for(unsigned i=0;i<lume_locale.ui_count;i++)if(!strcmp(lume_locale.ui[i].key,key)&&lume_locale.ui[i].value[0])return lume_locale.ui[i].value;
 return fallback;
}
static inline void lume_locale_load(void){
 lume_locale.font_ready=false;lume_locale.glyphs=lume_locale.ui_count=0;
 FILE *f=lume_locale_file("font.bin");uint32_t h[2];
 if(f){
  bool ok=fread(h,4,2,f)==2&&!memcmp(h,"LUF1",4)&&h[1]>=95&&h[1]<=LUME_LOCALE_GLYPHS;
  if(ok){unsigned n=h[1];ok=fread(lume_locale.codes,4,n,f)==n&&fread(lume_locale.font,240,n,f)==n&&fgetc(f)==EOF;
   for(unsigned i=0;ok&&i<95;i++)ok=lume_locale.codes[i]==i+32;
   for(unsigned i=95;ok&&i<n;i++){
    uint32_t c=lume_locale.codes[i];ok=c>=128&&c<=0x10ffff&&!(c>=0xd800&&c<=0xdfff);
    for(unsigned j=0;ok&&j<i;j++)if(lume_locale.codes[j]==c)ok=false;
   }
   if(ok){lume_locale.glyphs=n;lume_locale.font_ready=true;}
  }fclose(f);
 }
 f=lume_locale_file("ui.txt");
 if(f){char line[768];while(lume_locale.ui_count<LUME_LOCALE_UI&&fgets(line,sizeof line,f)){
   char *tab=strchr(line,'\t');if(!tab)continue;*tab++=0;size_t n=strlen(line);if(!n||n>=32)continue;
   tab[strcspn(tab,"\r\n")]=0;LumeTranslation *t=&lume_locale.ui[lume_locale.ui_count++];strcpy(t->key,line);lume_locale_encode(t->value,sizeof t->value,tab);
  }fclose(f);
 }lume_locale.serial++;
}
static inline uint32_t lume_settings_sum(const uint8_t *p,unsigned n){uint32_t h=2166136261u;for(unsigned i=0;i<n;i++)h=(h^p[i])*16777619u;return h;}
static inline bool lume_settings_save(void){
 uint8_t b[27]={0};memcpy(b,"LPS3",4);if(lume_locale.count)memcpy(b+4,lume_locale.languages[lume_locale.selected].id,16);
 b[20]=lume_locale.music;b[21]=lume_locale.effects;b[22]=lume_locale.stereo_percent>=250?3:lume_locale.stereo_percent>=150?2:1;uint32_t h=lume_settings_sum(b,23);memcpy(b+23,&h,4);
 char p[300],tmp[300],bak[300];
 if(!lume_ui_user_path(p,sizeof p,"settings.cfg")||!lume_ui_user_path(tmp,sizeof tmp,"settings.tmp")||!lume_ui_user_path(bak,sizeof bak,"settings.bak"))return false;
 FILE *f=fopen(tmp,"wb");if(!f)return false;bool ok=fwrite(b,1,sizeof b,f)==sizeof b;if(fclose(f))ok=false;if(!ok){remove(tmp);return false;}
 bool moved=false;f=fopen(p,"rb");if(f){fclose(f);remove(bak);if(rename(p,bak)){remove(tmp);return false;}moved=true;}
 if(rename(tmp,p)){if(moved)rename(bak,p);return false;}return true;
}
static inline void lume_locale_init(void){
 lume_locale.count=0;
 char user_list[300];if(!lume_ui_user_path(user_list,sizeof user_list,"lang/languages.txt"))user_list[0]=0;
 const char *lists[]={NULL,user_list};
 for(unsigned k=0;k<2;k++){FILE *f=k?fopen(lists[k],"rb"):lume_ui_asset_open("lang/languages.txt");if(!f)continue;char line[160];
  while(lume_locale.count<LUME_LOCALE_LANGS&&fgets(line,sizeof line,f)){
   char *tab=strchr(line,'\t');if(!tab)continue;*tab++=0;tab[strcspn(tab,"\r\n")]=0;if(!lume_locale_id(line)||strlen(tab)>63)continue;
   unsigned i=0;for(;i<lume_locale.count;i++)if(!strcmp(lume_locale.languages[i].id,line))break;
   strcpy(lume_locale.languages[i].id,line);strcpy(lume_locale.languages[i].name,tab);if(i==lume_locale.count)lume_locale.count++;
  }fclose(f);
 }
 if(!lume_locale.count){strcpy(lume_locale.languages[0].id,"en");strcpy(lume_locale.languages[0].name,"English");lume_locale.count=1;}
 lume_locale.selected=0;
 for(unsigned i=0;i<lume_locale.count;i++)if(!strcmp(lume_locale.languages[i].id,"en")){lume_locale.selected=i;break;}
 lume_locale.music=lume_locale.effects=lume_locale.stereo_percent=100;
 char setting[300],backup[300];if(!lume_ui_user_path(setting,sizeof setting,"settings.cfg"))setting[0]=0;
 if(!lume_ui_user_path(backup,sizeof backup,"settings.bak"))backup[0]=0;
 const char *paths[]={setting,backup};
 for(unsigned k=0;k<2;k++){
  FILE *f=fopen(paths[k],"rb");if(!f)continue;
  uint8_t b[28]={0};size_t n=fread(b,1,sizeof b,f);bool io_ok=!ferror(f);fclose(f);
  bool old=n==26&&!memcmp(b,"LPS1",4),previous=n==27&&!memcmp(b,"LPS2",4),current=n==27&&!memcmp(b,"LPS3",4);
  if(!io_ok||(!old&&!previous&&!current)||b[19]!=0||b[20]>100||b[21]>100||(previous&&b[22]>200)||(current&&(b[22]<1||b[22]>3)))continue;
  unsigned length=old?22:23;uint32_t h=0;memcpy(&h,b+length,4);
  if(h!=lume_settings_sum(b,length))continue;
  for(unsigned i=0;i<lume_locale.count;i++)if(!strcmp((char*)b+4,lume_locale.languages[i].id))lume_locale.selected=i;
  lume_locale.music=b[20];lume_locale.effects=b[21];lume_locale.stereo_percent=old?100:current?b[22]*100:b[22]>=150?200:100;break;
 }lume_locale_load();
}
/* The same source glyph atlas is used by menu labels and readable speech.
   Text here is already encoded to one glyph index per byte on pack load. */
static inline void lume_locale_draw(uint8_t *rgb,unsigned width,int x,int y,const char *text,uint32_t color,unsigned scale){
 for(;*text;text++,x+=6*(int)scale){unsigned c=(uint8_t)*text;if(c<32||c>=32+lume_locale.glyphs)c='?';
  if(c==' ')continue;
  const uint8_t *glyph=lume_locale.font+(c-32)*240;
  for(unsigned yy=0;yy<10*scale;yy++)for(unsigned xx=0;xx<6*scale;xx++){
   unsigned alpha=scale==2?glyph[yy*12+xx]:(glyph[yy*24+xx*2]+glyph[yy*24+xx*2+1]+glyph[yy*24+xx*2+12]+glyph[yy*24+xx*2+13]+2)/4;
   int tx=x+xx,ty=y+yy;if(!alpha||tx<0||ty<0||(unsigned)tx>=width||ty>=240)continue;
   uint8_t *p=rgb+(ty*width+tx)*3;uint8_t ink[]={color>>16&255,color>>8&255,color&255};lume_blend_pixel(p,ink,alpha);
  }
 }
}
#endif
