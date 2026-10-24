"""Run actual popup/draw handlers: reader dialogue must not leave a legacy paper."""
from pathlib import Path
import unittest,os,subprocess,tempfile
from test_fence_flow import ROOT,compiler,block
class ReadablePopupTests(unittest.TestCase):
 def test_solar_reader_suppresses_legacy_paper_and_retains_effects(self):
  cc=compiler()
  if not cc:self.skipTest('Native C compiler unavailable')
  main=(ROOT/'source/main.c').read_text(encoding='utf-8')
  prefix=r"""
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
typedef struct {bool active;} Reader;
typedef struct {FILE *file;unsigned frames,width,height;} LumeAnimation;
static Reader reading;static LumeAnimation speech_clip;static bool reading_replace,speech_visual_feedback,speech_ready,speech_ack,speech_missing,available=true;
static unsigned opens,draws,speech_frame;static uint64_t speech_started;static char detail_paper_name[80];static uint8_t speech[4];
static void say(const char *s){reading.active=reading_replace=available&&s;speech_visual_feedback=false;speech_ready=true;speech_clip.file=NULL;}
static void lume_read_close(Reader *r){r->active=false;}
static void lume_log(const char *s,...){(void)s;}
static bool lume_animation_open(LumeAnimation *c,const char *s){(void)s;opens++;c->file=(FILE*)1;c->frames=3;c->width=c->height=1;return true;}
static bool clip_has_art(LumeAnimation *c){(void)c;return true;}
static uint64_t osGetTime(void){return 100;}
static void lume_animation_close(LumeAnimation *c){c->file=NULL;}
static bool lume_animation_frame(LumeAnimation *c,unsigned n,uint8_t *p,int eye){(void)c;(void)n;(void)p;(void)eye;draws++;return true;}
static void lume_draw_text(uint8_t *p,int x,int y,const char *s,unsigned colour,unsigned scale){(void)p;(void)x;(void)y;(void)s;(void)colour;(void)scale;}
static void overlay(uint8_t *p,const uint8_t *s,unsigned w,unsigned h,int x,int y){(void)p;(void)s;(void)w;(void)h;(void)x;(void)y;}
#define CHECK(x) do{if(!(x)){printf("FAIL %d\n",__LINE__);return 1;}}while(0)
"""
  body=r"""
int main(void){uint8_t rgb[3]={4,5,6};
 popup_say("solar_win","rotationPuzzleWin");CHECK(reading.active&&reading_replace&&!speech_visual_feedback&&opens==0);
 draw_speech(rgb);CHECK(speech_ack&&draws==0&&rgb[0]==4&&rgb[1]==5&&rgb[2]==6);
 available=false;popup_say("solar_win","rotationPuzzleWin");CHECK(!reading.active&&opens==1);draw_speech(rgb);CHECK(draws==1);
 available=true;popup_say("wires_correct",NULL);CHECK(speech_visual_feedback&&opens==2);draw_speech(rgb);CHECK(draws==2);
 popup_say("power_fade","fullPower");CHECK(!reading.active&&!reading_replace&&speech_visual_feedback&&opens==3);draw_speech(rgb);CHECK(draws==3);
 puts("PASS solar readable-only paper, missing-reader fallback, wires feedback and power fade");return 0;
}
"""
  code=prefix+block(main,'static void popup_say(')+block(main,'static void draw_speech(')+body
  with tempfile.TemporaryDirectory(prefix='readable_popups_',dir=ROOT/'work') as folder:
   temp=Path(folder);source=temp/'test.c';source.write_text(code,encoding='utf-8');exe=temp/('test.exe' if os.name=='nt' else 'test');path,env,msvc=cc
   args=([path,'/nologo','/std:c11','/utf-8','/O2','/MT',str(source),'/Fe:'+str(exe)] if msvc else [path,'-O2','-std=c11',str(source),'-o',str(exe)])
   p=subprocess.run(args,cwd=temp,env=env,capture_output=True,text=True);self.assertEqual(p.returncode,0,p.stdout+p.stderr)
   p=subprocess.run([str(exe)],cwd=temp,env=env,capture_output=True,text=True);self.assertEqual(p.returncode,0,p.stdout+p.stderr);print(p.stdout.strip())
if __name__=='__main__':unittest.main()
