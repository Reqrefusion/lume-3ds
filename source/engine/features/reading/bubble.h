#ifndef LUME_READING_BUBBLE_H
#define LUME_READING_BUBBLE_H
#include "engine/render/colour.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "engine/features/ui/backend.h"
#include "engine/features/locale/locale.h"
/* Original English comes from the SWF; translations are separate UTF-8 packs.
   Convert to glyph codes once, keeping fixed20px cells for readable wrapping. */
#include <stdlib.h>
#define LUME_READING_LIMIT 8192
#define LUME_READING_LINES 7
#define LUME_READING_PAGES 128
typedef struct {
 char text[LUME_READING_LIMIT+1];unsigned offsets[LUME_READING_PAGES];
 unsigned page,pages,width,serial,painted;bool active,finished,presented,cancel,structured,document,timed;
 uint8_t canvas[400*240*3],alpha[400*240],font[LUME_LOCALE_GLYPHS*12*20];
 bool font_checked,font_ready,glyph_ready[LUME_LOCALE_GLYPHS];unsigned language_serial;
 uint8_t *papers[2];bool paper_checked[2];
} LumeReading;
static inline void lume_read_close(LumeReading *r) {
 r->active=false;r->finished=false;r->presented=false;r->page=r->pages=0;
 r->width=0;r->structured=false;r->document=false;r->timed=false;r->serial++;
}
/* Source paper pixels are immutable across pages, labels and languages.
   Two bounded buffers retain exact premultiplied RGB and alpha (691200 B). */
static inline void lume_read_release(LumeReading *r) {
 for(unsigned i=0;i<2;i++){free(r->papers[i]);r->papers[i]=NULL;r->paper_checked[i]=false;}
}
static inline bool lume_read_paper(LumeReading *r,unsigned width) {
 if(width!=320&&width!=400)return false;
 unsigned index=width==400,bytes=width*240*4;
 if(!r->paper_checked[index]) {
  r->paper_checked[index]=true;
  char path[96];snprintf(path,sizeof path,"detail/reading/paper%u.bin",width);
  FILE *f=lume_ui_asset_open(path);
  if(f){
   uint8_t h[8];bool ok=fread(h,1,8,f)==8&&!memcmp(h,"LRP2",4)&&
    (unsigned)(h[4]|h[5]<<8)==width&&h[6]==240&&h[7]==0;
   uint8_t *pixels=ok?malloc(bytes):NULL;
   if(pixels&&(fread(pixels,1,bytes,f)!=bytes||fgetc(f)!=EOF||ferror(f))){free(pixels);pixels=NULL;}
   fclose(f);r->papers[index]=pixels;
  }
 }
 if(!r->papers[index])return false;
 memcpy(r->canvas,r->papers[index],width*240*3);
 memcpy(r->alpha,r->papers[index]+width*240*3,width*240);return true;
}
static inline void lume_read_font(LumeReading *r) {
 if(r->font_checked&&r->language_serial==lume_locale.serial)return;
 r->language_serial=lume_locale.serial;r->font_ready=false;memset(r->font,0,sizeof r->font);memset(r->glyph_ready,0,sizeof r->glyph_ready);
 if(lume_locale.font_ready){
  memcpy(r->font,lume_locale.font,lume_locale.glyphs*240);r->font_checked=r->font_ready=true;
  for(unsigned c=0;c<lume_locale.glyphs;c++)for(unsigned i=0;i<240;i++)if(r->font[c*240+i]){r->glyph_ready[c]=true;break;}
  return;
 }
 r->font_checked=true;FILE *f=lume_ui_asset_open("detail/reading/font.bin");if(!f)return;
 uint8_t h[8];r->font_ready=fread(h,1,8,f)==8&&!memcmp(h,"LRF1\014\024\137\000",8)&&
  fread(r->font,1,95*240,f)==95*240&&fgetc(f)==EOF;fclose(f);
 if(r->font_ready)for(unsigned c=0;c<95;c++)for(unsigned i=0;i<240;i++)
  if(r->font[c*240+i]){r->glyph_ready[c]=true;break;}
}
static inline bool lume_read_open(LumeReading *r,const char *label) {
 lume_read_close(r);if(!label)return false;
 lume_read_font(r);
 size_t n=strlen(label);if(!n||n>64)return false;
 for(size_t i=0;i<n;i++)if(!((label[i]>='a'&&label[i]<='z')||(label[i]>='A'&&label[i]<='Z')||(label[i]>='0'&&label[i]<='9')||label[i]=='_'))return false;
 char path[128];snprintf(path,sizeof path,"detail/reading/%s.txt",label);
 char file[80];snprintf(file,sizeof file,"%s.txt",label);
 FILE *f=lume_locale_file(file);
 if(!f)f=lume_ui_asset_open(path);
 if(!f){snprintf(path,sizeof path,"lang/en/%s",file);f=lume_ui_asset_open(path);}
 if(!f)return false;
 size_t bytes=fread(r->text,1,LUME_READING_LIMIT,f);int extra=fgetc(f);bool ok=!ferror(f)&&extra==EOF;fclose(f);
 if(!ok)return false;
 r->document=!strncmp(label,"doc_",4);
 bool structured=r->document||!strncmp(label,"page_",5)||!strcmp(label,"help_map")||!strcmp(label,"help_solar");
 unsigned out=0;bool space=false;
 r->text[bytes]=0;const char *cursor=r->text;
 while(*cursor){
  uint32_t point=lume_utf8(&cursor);if(point==0xfffd)return false;
  unsigned ch=lume_locale_char(point);
  if(structured&&ch=='\r')continue;
  if(structured&&ch=='\n'){
   if(out)r->text[out++]='\n';
   space=false;continue;
  }
  if(ch<=32){if(out&&(!structured||r->text[out-1]!='\n'))space=true;continue;}
  if(ch>=32+LUME_LOCALE_GLYPHS)return false;
  if(space)r->text[out++]=' ';
  space=false;r->text[out++]=(char)ch;
 }
 while(out&&r->text[out-1]=='\n')out--;
 r->structured=structured;r->text[out]=0;r->active=out>0;r->cancel=false;return r->active;
}
static inline unsigned lume_read_line(const char *text,unsigned at,unsigned columns,char *line) {
 while(text[at]==' ')at++;
 unsigned end=at,last=at;
 while(text[end]&&text[end]!='\n'&&end-at<columns){if(text[end]==' ')last=end;end++;}
 if(text[end]&&text[end]!=' '&&text[end]!='\n'&&last>at)end=last;
 unsigned n=end-at;if(line){memcpy(line,text+at,n);line[n]=0;}
 while(text[end]==' ')end++;
 if(text[end]=='\n')end++;
 return end;
}
static inline unsigned lume_read_columns(const LumeReading *r,unsigned width) {
 return (width-(r->structured?40:56))/12;
}
static inline void lume_read_layout(LumeReading *r,unsigned width) {
 if(r->width==width)return;
 unsigned old=r->pages?r->offsets[r->page]:0,at=0;r->pages=0;r->page=0;
 while(r->text[at]&&r->pages<LUME_READING_PAGES){
  unsigned p=r->pages++;r->offsets[p]=at;if(at<=old)r->page=p;
  for(unsigned line=0;line<LUME_READING_LINES&&r->text[at];line++)at=lume_read_line(r->text,at,lume_read_columns(r,width),NULL);
 }
 r->width=width;r->presented=false;r->serial++;
}
static inline bool lume_read_step(LumeReading *r,bool back) {
 if(!r->active||!r->presented||r->finished)return false;
 if(back&&r->page){r->page--;r->presented=false;r->serial++;return true;}
 if(!back&&r->page+1<r->pages){r->page++;r->presented=false;r->serial++;return true;}
 r->finished=true;r->cancel=back;r->serial++;return true;
}
static inline void lume_read_ink(LumeReading *r,unsigned width,int x,int y,const char *s,unsigned scale) {
 uint8_t *rgb=r->canvas;
 for(;*s;s++,x+=(int)(6*scale)){
  unsigned ch=(unsigned char)*s;if(ch<32||ch>=32+LUME_LOCALE_GLYPHS)ch='?';
  bool native=(scale==1||scale==2)&&r->font_ready&&r->glyph_ready[ch-32];
  unsigned gw=native?6*scale:6,gh=native?10*scale:10,repeat=native?1:scale;
  for(unsigned yy=0;yy<gh;yy++)for(unsigned xx=0;xx<gw;xx++){
   unsigned a;
   if(native&&scale==2)a=r->font[(ch-32)*240+yy*12+xx];
   else if(native){unsigned i=(ch-32)*240+yy*24+xx*2;a=(r->font[i]+r->font[i+1]+r->font[i+12]+r->font[i+13]+2)/4;}
   else a=lume_ui_fallback_alpha(ch,yy*6+xx);
   if(!a)continue;
   for(unsigned dy=0;dy<repeat;dy++)for(unsigned dx=0;dx<repeat;dx++){
    int tx=x+(int)(xx*repeat+dx),ty=y+(int)(yy*repeat+dy);
    if(tx<0||ty<0||(unsigned)tx>=width||ty>=240)continue;
    uint8_t *p=rgb+((unsigned)ty*width+(unsigned)tx)*3;
    const uint8_t ink[3]={40,27,18};lume_blend_pixel(p,ink,a);
    unsigned pixel=(unsigned)ty*width+(unsigned)tx;r->alpha[pixel]=a+(r->alpha[pixel]*(255-a)+127)/255;
   }
  }
 }
}
static inline const uint8_t *lume_read_draw(LumeReading *r,unsigned width) {
 lume_read_layout(r,width);
 if(r->painted==r->serial)return r->canvas;
 bool paper_ready=lume_read_paper(r,width);
 if(!paper_ready)for(unsigned y=0;y<240;y++)for(unsigned x=0;x<width;x++){
  unsigned pixel=y*width+x;uint8_t *p=r->canvas+pixel*3;
  bool inside=x>=8&&x<width-8&&y>=8&&y<208;
  r->alpha[pixel]=inside?255:0;p[0]=inside?235:0;p[1]=inside?230:0;p[2]=inside?214:0;
 }
 /* A card may prefer a whole instruction at the next page boundary. Limit
    this page without discarding any source text or affecting normal wraps. */
 unsigned stop=r->page+1<r->pages?r->offsets[r->page+1]:(unsigned)strlen(r->text);
 char saved=r->text[stop];r->text[stop]=0;
 unsigned at=r->offsets[r->page],rows=0;char lines[LUME_READING_LINES][64];
 for(;rows<LUME_READING_LINES&&r->text[at];rows++)
  at=lume_read_line(r->text,at,lume_read_columns(r,width),lines[rows]);
 r->text[stop]=saved;
 /* Original speech is centered. Keep the readable letter size, centering
    each wrapped line and the whole block inside the enlarged source paper. */
 int top=r->structured?24:24+(168-(int)rows*24)/2;
 for(unsigned n=0;n<rows;n++)
  lume_read_ink(r,width,(r->document||(r->structured&&!(r->page==0&&n==0)))?20:
    ((int)width-(int)strlen(lines[n])*12)/2,top+(int)n*24,lines[n],2);
 char footer[96];
 if(r->document)snprintf(footer,sizeof footer,"%u/%u Y:%s L:%s R:%s",r->page+1,r->pages,r->page+1<r->pages?lume_tr("next","Next"):lume_tr("close","Close"),lume_tr("lower","Lower"),lume_tr("upper","Upper"));
 else snprintf(footer,sizeof footer,"%u/%u  %s:%s  B:%s",r->page+1,r->pages,lume_tr("touch","Touch/A"),r->page+1<r->pages?lume_tr("next","Next"):lume_tr("close","Close"),r->page?lume_tr("previous","Previous"):lume_tr("back","Back"));
 if(r->document)for(unsigned y=216;y<235;y++)for(unsigned x=16;x<width-16;x++){
  unsigned pixel=y*width+x;r->alpha[pixel]=225;
  r->canvas[pixel*3]=207;r->canvas[pixel*3+1]=203;r->canvas[pixel*3+2]=189;
 }
 if(!r->timed)lume_read_ink(r,width,((int)width-(int)strlen(footer)*6)/2,221,footer,1);
 r->painted=r->serial;return r->canvas;
}
/* Composite onto the fresh game image, not the previous composed frame.
   RGB is premultiplied, so transparent paper edges retain the source alpha. */
static inline void lume_read_overlay(const LumeReading *r,uint8_t *rgb) {
 for(unsigned i=0;i<r->width*240;i++){
  unsigned a=r->alpha[i];if(!a)continue;
  lume_blend_premultiplied(rgb+i*3,r->canvas+i*3,a);
 }
}
static inline void lume_read_top(const LumeReading *r,uint8_t *framebuffer) {
 for(unsigned y=0;y<240;y++)for(unsigned x=0;x<400;x++){
  unsigned pixel=y*400+x,a=r->alpha[pixel];if(!a)continue;
  const uint8_t *p=r->canvas+pixel*3;uint8_t *d=framebuffer+(x*240+239-y)*3;
  const uint8_t bgr[3]={p[2],p[1],p[0]};lume_blend_premultiplied(d,bgr,a);
 }
}
#endif
