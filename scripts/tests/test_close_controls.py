"""Exercise separate bottom UI and actual X routing without resetting puzzles."""
import unittest
import test_exception_paths as native
from test_fence_flow import ROOT,block
class CloseControlTests(unittest.TestCase):
 def test_bottom_ui_and_x_routes(self):
  source=ROOT/'source'
  main=(source/'main.c').read_text()
  route=block(main,'    if((down&KEY_X)&&!animation.file')
  program='#include "'+(source/'game/lume/puzzles.c').as_posix()+'"\n#include "'+(source/'game/lume/game.c').as_posix()+'"\n#include "'+(source/'game/lume/close_controls.h').as_posix()+'"\n'+r'''
#include <assert.h>
#define KEY_X 1u
#define KEY_TOUCH 2u
static LumeGame game;
static unsigned down,held,visual_epoch;
static bool debug,reading_instruction,ending_hold,reading_touch_latch;
static struct {bool file;} animation;
static struct {int mode;} puzzle_flow={-1},native_menu;
static int reading;
static void lume_read_close(int *r){*r=0;}
static void say(const char *s){(void)s;}
static void close_clue(LumeGame *g){g->clue=-1;}
static void dispatch(LumeGame *g,int action){assert(lume_game_action(g,action));}
static void press(void){down=KEY_X;held=KEY_TOUCH;
'''+route+r'''
}
int main(void){
 for(int mode=0;mode<LUME_PUZZLE_COUNT;mode++){
  lume_game_new(&game,123);game.puzzle=mode;game.inventory=1u<<KEYS;
  game.puzzles.keys.selected=7;LumePuzzles before=game.puzzles;
  press();
  assert(game.puzzle==-1&&game.inventory==(1u<<KEYS)&&!memcmp(&before,&game.puzzles,sizeof before));
  assert(!down&&!(held&KEY_TOUCH)&&reading_touch_latch);
 }
 for(int clue=0;clue<10;clue++){
  lume_game_new(&game,123);game.clue=clue;assert(lume_close_target(&game,false,false)==LUME_CLOSE_CLUE);press();assert(game.clue==-1);
 }
 lume_game_new(&game,123);game.scene=PANTRY;press();assert(game.scene==HALL);
 game.scene=ROOF;press();assert(down==KEY_X); /* No close target: no world action. */
 game.puzzle=LUME_KEYS;reading_instruction=true;reading=1;press();
 assert(!reading_instruction&&!reading&&game.puzzle==LUME_KEYS); /* Help closes first. */
 puzzle_flow.mode=LUME_KEYS;press();assert(game.puzzle==LUME_KEYS&&down==KEY_X);
 puzzle_flow.mode=-1;animation.file=true;press();assert(game.puzzle==LUME_KEYS);
 animation.file=false;native_menu.mode=1;press();assert(game.puzzle==LUME_KEYS);native_menu.mode=0;
 static uint8_t bottom[320*240*3];memset(bottom,0xa5,sizeof bottom);
 lume_close_draw_bottom(bottom);unsigned changed=0;
 for(unsigned y=0;y<240;y++)for(unsigned x=0;x<320;x++){
  unsigned at=(x*240+239-y)*3;bool inside=x>=lume_close_x&&x<lume_close_x+lume_close_w&&y>=lume_close_y&&y<lume_close_y+lume_close_h;
  if(!inside)assert(bottom[at]==0xa5&&bottom[at+1]==0xa5&&bottom[at+2]==0xa5);
  else if(bottom[at]!=0xa5||bottom[at+1]!=0xa5||bottom[at+2]!=0xa5)changed++;
 }
 assert(changed>100);
 return 0;
}
'''
  self.assertNotIn('lume_close_patch_top',main)
  self.assertNotIn('bool reset = down & KEY_X',main)
  self.assertIn('if(d&(KEY_B|KEY_X))keys|=LM_BACK',main)
  header='#include "'+(source/'game/lume/close_controls.h').as_posix()+'"'
  wrapped='#include <stdio.h>\nstatic FILE *corner_open(const char *p,const char *m){(void)p;return fopen("'+(ROOT/'romfs/ui_close.bin').as_posix()+'",m); }\n#define fopen corner_open\n'+header+'\n#undef fopen\n'
  program=program.replace(header,wrapped).replace('int main(void){','int main(void){\n assert(lume_close_load());')
  native.ExceptionPathTests().native_check(program)
