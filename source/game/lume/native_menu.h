#ifndef LUME_NATIVE_MENU_H
#define LUME_NATIVE_MENU_H
#include "game.h"
#include "render.h"
#include "../../features/ui/menu_art.h"
#include <string.h>
/* System buttons own these lower-screen overlays. Input never reaches world
   hotspots or puzzle controls while an overlay is opening, open, or closing. */
enum { LUME_MENU_CLOSED, LUME_MENU_INVENTORY, LUME_MENU_SYSTEM };
enum { LUME_MENU_NONE, LUME_MENU_SAVE, LUME_MENU_QUIT };
enum { LM_SELECT=1,LM_START=2,LM_BACK=4,LM_OK=8,LM_UP=16,LM_DOWN=32,
       LM_LEFT=64,LM_RIGHT=128,LM_TOUCH=256 };
typedef struct { unsigned mode,focus,serial;char notice[40]; } LumeNativeMenu;
static const int lume_menu_items[7]={WIRE,SPANNER,BLADES,POLES,DYNAMO,BINOCULARS,KEYS};
static const char *const lume_menu_names[7]={"Wire","Spanner","Blades","Poles","Dynamo","Binoculars","Keys"};
static inline bool lume_menu_owned(const LumeGame *g,unsigned slot) {
 return slot<7&&(g->inventory&(1u<<lume_menu_items[slot]));
}
static inline void lume_menu_close(LumeNativeMenu *m) {
 m->mode=LUME_MENU_CLOSED;m->notice[0]=0;m->serial++;
}
static inline void lume_menu_open(LumeNativeMenu *m,const LumeGame *g,unsigned mode) {
 m->mode=mode;m->focus=0;m->notice[0]=0;m->serial++;
 if(mode==LUME_MENU_INVENTORY){
  for(unsigned i=0;i<7;i++)if(lume_menu_owned(g,i)){m->focus=i;break;}
  for(unsigned i=0;i<7;i++)if(lume_menu_owned(g,i)&&g->selected==lume_menu_items[i])m->focus=i;
 }
}
static inline unsigned lume_menu_input(LumeNativeMenu *m,LumeGame *g,unsigned keys,int x,int y,bool stable) {
 if(keys&(LM_START|LM_SELECT)){
  unsigned mode=keys&LM_START?LUME_MENU_SYSTEM:LUME_MENU_INVENTORY;
  if(m->mode==mode)lume_menu_close(m);else lume_menu_open(m,g,mode);
  return LUME_MENU_NONE;
 }
 if(!m->mode)return LUME_MENU_NONE;
 if(keys&LM_BACK){lume_menu_close(m);return LUME_MENU_NONE;}
 unsigned old=m->focus;
 if(m->mode==LUME_MENU_INVENTORY){
  if(keys&(LM_UP|LM_DOWN)){
   int row=(int)(old/4)+((keys&LM_UP)?-1:1),column=(int)(old%4);
   if(row>=0&&row<2)for(int distance=0;distance<4;distance++){
    bool found=false;
    for(int side=0;side<2;side++){
     int col=column+(side?distance:-distance);if(col<0||col>=4)continue;
     unsigned slot=(unsigned)(row*4+col);
     if(lume_menu_owned(g,slot)){m->focus=slot;found=true;break;}
    }
    if(found)break;
   }
  }else if(keys&(LM_LEFT|LM_RIGHT)){
   int step=(keys&LM_LEFT)?-1:1,row=(int)(old/4);
   for(int col=(int)(old%4)+step;col>=0&&col<4;col+=step){
    unsigned slot=(unsigned)(row*4+col);if(lume_menu_owned(g,slot)){m->focus=slot;break;}
   }
  }
  bool choose=keys&LM_OK;
  if((keys&LM_TOUCH)&&x>=8&&x<308&&y>=44&&y<208){
   unsigned col=(x-8)/76,row=(y-44)/84,slot=row*4+col;
   if((x-8)%76<72&&(y-44)%84<80&&lume_menu_owned(g,slot)){m->focus=slot;choose=true;}
  }
  if(choose&&lume_menu_owned(g,m->focus)){
   int item=lume_menu_items[m->focus];g->selected=g->selected==item?-1:item;
   lume_menu_close(m);return LUME_MENU_NONE;
  }
 }else{
  if(keys&(LM_UP|LM_DOWN|LM_LEFT|LM_RIGHT))m->focus=1-m->focus;
  bool choose=keys&LM_OK;
  if((keys&LM_TOUCH)&&x>=52&&x<268){
   if(y>=48&&y<128){m->focus=0;choose=true;}
   else if(y>=136&&y<216){m->focus=1;choose=true;}
  }
  if(choose){
   if(stable)return m->focus?LUME_MENU_QUIT:LUME_MENU_SAVE;
   strcpy(m->notice,"Finish the transition first");m->serial++;
  }
 }
 if(old!=m->focus){m->notice[0]=0;m->serial++;}
 return LUME_MENU_NONE;
}
static inline void lume_menu_box(uint8_t *rgb,int x,int y,int w,int h,uint32_t color) {
 for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++){
  if((unsigned)xx>=320||(unsigned)yy>=240)continue;
  uint8_t *p=rgb+(yy*320+xx)*3;p[0]=color>>16;p[1]=color>>8;p[2]=color;
 }
}
static inline void lume_menu_text(const LumeMenuArt *art,uint8_t *rgb,int y,const char *s,uint32_t color,int scale) {
 lume_menu_art_text(art,rgb,(320-(int)strlen(s)*6*scale)/2,y,s,color,(unsigned)scale);
}
static inline void lume_menu_draw(const LumeNativeMenu *m,const LumeGame *g,const LumeMenuArt *art,uint8_t *rgb) {
 if(!m->mode)return;
 for(unsigned i=0;i<320*240*3;i++)rgb[i]=(rgb[i]*96+127)/255;
 if(m->mode==LUME_MENU_INVENTORY){
  if(art&&art->ready&&!strcmp(lume_locale.languages[lume_locale.selected].id,"en"))lume_menu_art_blit(art,7,rgb,60,8);
  else lume_menu_text(art,rgb,10,lume_tr("inventory","Inventory"),0xf6e8c9,2);
  if(!g->inventory)lume_menu_text(art,rgb,100,lume_tr("empty_inventory","Inventory is empty"),0xf6e8c9,2);
  else for(unsigned i=0;i<7;i++)if(lume_menu_owned(g,i)){
   int x=8+(i%4)*76,y=44+(i/4)*84;
   lume_menu_art_blit(art,i,rgb,x+4,y+2);
   // Controller focus is marked outside the original tile, not redrawn over it.
   if(i==m->focus){lume_menu_box(rgb,x,y,2,8,0xf6e8c9);lume_menu_box(rgb,x,y,8,2,0xf6e8c9);}
   static const char *const keys[]={"wire","spanner","blades","poles","dynamo","binoculars","keys"};
   const char *name=lume_tr(keys[i],lume_menu_names[i]);
   lume_menu_art_text(art,rgb,x+(72-(int)strlen(name)*6)/2,y+67,name,g->selected==lume_menu_items[i]?0xe6ad48:0xf6e8c9,1);
  }
 }else{
  lume_menu_text(art,rgb,10,lume_tr("save_quit","Save / Quit"),0xf6e8c9,2);
  lume_menu_art_blit(art,!strcmp(m->notice,"Saved")?9:8,rgb,52,48);
  lume_menu_art_blit(art,10,rgb,52,136);
  lume_menu_text(art,rgb,76,lume_tr(!strcmp(m->notice,"Saved")?"saved":"save",!strcmp(m->notice,"Saved")?"Saved":"Save"),0x8bc4a4,2);
  lume_menu_text(art,rgb,164,lume_tr("quit","Quit"),0xc7b898,2);
  int y=m->focus?166:78;
  for(int i=0;i<7;i++)lume_menu_box(rgb,34+i,y+i,2,15-i*2,0xf6e8c9);
 }
 const char *notice=m->notice;
 if(!strcmp(notice,"Saving..."))notice=lume_tr("saving","Saving...");
 if(!strcmp(notice,"Saved"))notice=lume_tr("saved","Saved");
 else if(!strcmp(notice,"Save failed - try again"))notice=lume_tr("save_error","Save failed - try again");
 else if(!strcmp(notice,"Finish the transition first"))notice=lume_tr("transition_wait","Finish the transition first");
 lume_menu_text(art,rgb,223,notice[0]?notice:m->mode==LUME_MENU_INVENTORY?lume_tr("inventory_keys","A: Select   B: Back"):lume_tr("confirm_keys","A: Confirm   B: Back"),0xf6e8c9,1);
}
#endif
