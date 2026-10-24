#ifndef LUME_TITLE_MENU_H
#define LUME_TITLE_MENU_H
#include <stdbool.h>
/* Pure menu state: selecting/cancelling never mutates a game or save file. */
enum { TITLE_MAIN, TITLE_CREDITS, TITLE_CONFIRM, TITLE_SETTINGS };
enum { TITLE_NONE, TITLE_CONTINUE, TITLE_NEW, TITLE_MUTE, TITLE_REPLAY, TITLE_QUIT };
enum { TITLE_UP=1, TITLE_DOWN=2, TITLE_OK=4, TITLE_BACK=8, TITLE_LEFT=16, TITLE_RIGHT=32, TITLE_Y=64 };
typedef struct { int page, selection, confirm; bool has_save, focused; } LumeTitleMenu;
static inline void lume_title_init(LumeTitleMenu *m, bool has_save) {
 m->page=TITLE_MAIN;m->selection=has_save?0:1;m->confirm=1;m->has_save=has_save;m->focused=false;
}
static inline int lume_title_activate(LumeTitleMenu *m) {
 if(m->page==TITLE_CREDITS){m->page=TITLE_MAIN;return TITLE_NONE;}
 if(m->page==TITLE_CONFIRM){if(!m->confirm)return TITLE_NEW;m->page=TITLE_MAIN;return TITLE_NONE;}
 if(m->selection==0)return m->has_save?TITLE_CONTINUE:TITLE_NONE;
 if(m->selection==3){m->page=TITLE_SETTINGS;m->focused=false;return TITLE_NONE;}
 if(m->selection==1){if(!m->has_save)return TITLE_NEW;m->page=TITLE_CONFIRM;m->confirm=1;m->focused=false;return TITLE_NONE;}
 m->page=TITLE_CREDITS;m->focused=false;return TITLE_NONE;
}
static inline int lume_title_key(LumeTitleMenu *m,unsigned keys) {
 if((keys&TITLE_Y)&&m->has_save&&m->page==TITLE_MAIN)return TITLE_REPLAY;
 if(keys&TITLE_BACK){if(m->page!=TITLE_MAIN){m->page=TITLE_MAIN;m->focused=false;}return TITLE_NONE;}
 if(keys&(TITLE_UP|TITLE_DOWN|TITLE_LEFT|TITLE_RIGHT)){
  m->focused=true;
  if(m->page==TITLE_CONFIRM)m->confirm^=1;
  else if(m->page==TITLE_MAIN){int d=(keys&(TITLE_UP|TITLE_LEFT))?-1:1;
   do{m->selection=(m->selection+d+4)%4;}while(!m->has_save&&m->selection==0);
  }
 }
 return keys&TITLE_OK?lume_title_activate(m):TITLE_NONE;
}
static inline int lume_title_touch(LumeTitleMenu *m,int x,int y) {
 if(x<0||x>=320||y<13||y>=227)return TITLE_NONE;

 if(m->page==TITLE_CREDITS){if(x>=140&&x<178&&y>=160&&y<177)m->page=TITLE_MAIN;return TITLE_NONE;}
 if(m->page==TITLE_CONFIRM){
  if(y>=140&&y<160){if(x>=113&&x<150){m->confirm=0;return TITLE_NEW;}if(x>=164&&x<201){m->confirm=1;m->page=TITLE_MAIN;}}
  return TITLE_NONE;
 }
 if(x<128||x>=194)return TITLE_NONE;
 int item=y>=116&&y<131?0:(y>=132&&y<147?1:(y>=147&&y<162?2:-1));
 if(item<0||(!m->has_save&&item==0))return TITLE_NONE;
 m->selection=item;m->focused=false;return lume_title_activate(m);
}
#endif
