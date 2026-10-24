#ifndef LUME_SETTINGS_UI_H
#define LUME_SETTINGS_UI_H
#include "../../engine/render/colour.h"
#include "../../features/locale/locale.h"
#include "native_menu.h"
#include "title_menu.h"
static inline void lume_ui_center(const LumeMenuArt *art,uint8_t *rgb,int y,const char *s,uint32_t color,unsigned scale){
 lume_menu_art_text(art,rgb,(320-(int)strlen(s)*6*(int)scale)/2,y,s,color,scale);
}
static inline void lume_ui_bar(uint8_t *rgb,int y,unsigned value,bool focus){
 lume_menu_box(rgb,48,y,224,8,focus?0x736951:0x403f36);lume_menu_box(rgb,48,y,value*224/100,8,0x83c5a5);
 lume_menu_box(rgb,45+value*224/100,y-4,6,16,0xf6e8c9);
}
static inline void lume_settings_draw(const LumeMenuArt *art,uint8_t *rgb,unsigned focus,bool save_error){
 lume_ui_center(art,rgb,5,lume_tr("settings","Settings"),0xf6e8c9,2);
 lume_ui_center(art,rgb,32,lume_tr("language","Language"),focus==0?0xf6ad48:0xf6e8c9,1);
 char name[80];lume_locale_encode(name,sizeof name,lume_locale.languages[lume_locale.selected].name);
 lume_ui_center(art,rgb,49,name,0x83c5a5,2);lume_menu_art_text(art,rgb,12,49,"<",0xf6e8c9,2);lume_menu_art_text(art,rgb,296,49,">",0xf6e8c9,2);
 char label[100];snprintf(label,sizeof label,"%s  %u%%",lume_tr("music","Music"),lume_locale.music);
 lume_ui_center(art,rgb,82,label,focus==1?0xf6ad48:0xf6e8c9,1);lume_ui_bar(rgb,101,lume_locale.music,focus==1);
 snprintf(label,sizeof label,"%s  %u%%",lume_tr("effects","Effects"),lume_locale.effects);
 lume_ui_center(art,rgb,124,label,focus==2?0xf6ad48:0xf6e8c9,1);lume_ui_bar(rgb,143,lume_locale.effects,focus==2);
 lume_ui_center(art,rgb,164,lume_tr("stereo_gain","3D strength"),focus==3?0xf6ad48:0xf6e8c9,1);
 unsigned level=lume_locale.stereo_percent>=250?2:lume_locale.stereo_percent>=150?1:0;
 const char *keys[]={"stereo_normal","stereo_high","stereo_very_high"};
 const char *names[]={"Normal","High","Very high"};
 snprintf(label,sizeof label,"%s",lume_tr(keys[level],names[level]));
 lume_ui_center(art,rgb,181,label,0x83c5a5,2);
 lume_menu_art_text(art,rgb,12,181,"<",focus==3?0xf6ad48:0xf6e8c9,2);
 lume_menu_art_text(art,rgb,296,181,">",focus==3?0xf6ad48:0xf6e8c9,2);
 lume_ui_center(art,rgb,225,save_error?lume_tr("settings_error","Settings save failed"):lume_tr("back","Back"),focus==4?0xf6ad48:0xf6e8c9,1);
}
/* Return true only when a preference changed. Back is handled by the caller;
   changing language never touches the gameplay save or starts a new game. */
static inline bool lume_settings_input(unsigned *focus,unsigned keys,bool touch,int x,int y){
 if(keys&TITLE_UP)*focus=(*focus+4)%5;
 if(keys&TITLE_DOWN)*focus=(*focus+1)%5;
 int direction=(keys&TITLE_LEFT)?-1:(keys&TITLE_RIGHT)?1:0;
 if(touch){
  if(y>=29&&y<76){*focus=0;direction=x<160?-1:1;}
  else if(y>=77&&y<118)*focus=1;
  else if(y>=119&&y<160)*focus=2;
  else if(y>=161&&y<201)*focus=3;
  else if(y>=214)*focus=4;
  else return false;
 }
 if(*focus==0&&((keys&TITLE_OK)||direction)){
  unsigned previous=lume_locale.selected;
  lume_locale.selected=(lume_locale.selected+lume_locale.count+(direction<0?-1:1))%lume_locale.count;
  if(previous!=lume_locale.selected){lume_locale_load();return true;}
 }
 if(*focus==3 && (direction||touch||(keys&TITLE_OK))) {
  unsigned level=lume_locale.stereo_percent>=250?2:lume_locale.stereo_percent>=150?1:0;
  if(touch)direction=x<160?-1:1;
  lume_locale.stereo_percent=((level+3+(direction<0?-1:1))%3+1)*100;
  lume_locale.serial++;return true;
 }
 if(*focus>=1&&*focus<=2){
  unsigned *level=*focus==1?&lume_locale.music:&lume_locale.effects;
  unsigned old=*level;int maximum=100;
  if(touch){int v=(x-48)*maximum/224;if(v<0)v=0;if(v>maximum)v=maximum;*level=((v+5)/10)*10;}
  else if(direction){int v=(int)*level+direction*10;if(v<0)v=0;if(v>maximum)v=maximum;*level=v;}
  if(old!=*level){lume_locale.serial++;return true;}
 }return false;
}
static inline int lume_title_main_y(bool save,int item){return save?24+48*item:48+52*(item-1);}
static inline void lume_title_main_draw(const LumeMenuArt *art,uint8_t *rgb,const LumeTitleMenu *m){
 const char *key[]={"continue","new_game","credits","settings"};const char *fallback[]={"Continue","New Game","Credits","Settings"};
 for(int i=m->has_save?0:1;i<4;i++)lume_ui_center(art,rgb,lume_title_main_y(m->has_save,i)+10,lume_tr(key[i],fallback[i]),i==m->selection?0xf79424:0x00cda5,2);
}
/* A selected item stays visible after Select closes. Native 64px artwork is
   sampled down to32px; the photographed scene and its detail camera stay clean. */
static inline void lume_selected_item(const LumeGame *g,LumeMenuArt *a,uint8_t *rgb){
 int slot=-1;for(int i=0;i<7;i++)if(g->selected==lume_menu_items[i]&&lume_menu_owned(g,i))slot=i;
 if(slot<0||!lume_menu_art_load(a))return;
 unsigned offset=(unsigned)slot*64*64*4;
 for(unsigned y=0;y<36;y++)for(unsigned x=0;x<36;x++){
  uint8_t *d=rgb+((y+198)*320+x+278)*3;for(unsigned k=0;k<3;k++)d[k]=d[k]*80/255;
 }
 for(unsigned y=0;y<32;y++)for(unsigned x=0;x<32;x++){
  const uint8_t *s=a->rgba+offset+((y*2)*64+x*2)*4;uint8_t *d=rgb+((y+200)*320+x+280)*3;
  lume_blend_premultiplied(d,s,s[3]);
 }
}
#endif
