"""Run alternate puzzle orders and resume paths against actual gameplay rules."""
import unittest
import test_exception_paths as native
from test_fence_flow import ROOT
class GameplayRouteTests(unittest.TestCase):
 def test_all_puzzle_orders_and_save_resume(self):
  source=ROOT/'source/game/lume'
  program='#include "'+(source/'puzzles.c').as_posix()+'"\n#include "'+(source/'game.c').as_posix()+'"\n'+r'''
#include <assert.h>
static LumeGame g;
static int runs;
static void act(int a){assert(lume_game_action(&g,a));}
static void solve(void){
 int p=g.puzzle;assert(p>=0);
 if(p==LUME_DOOR)memcpy(g.puzzles.door.digits,lume_door_code,sizeof lume_door_code);
 if(p==LUME_CUPBOARD){for(int i=0;i<3;i++){memcpy(g.puzzles.cupboard.digits,lume_cupboard_codes[i],sizeof lume_cupboard_codes[i]);assert(lume_submit(&g.puzzles,p));}}
 else {
  if(p==LUME_KEYS)g.puzzles.keys.selected=LUME_KEYS_TARGET;
  if(p==LUME_WIRES)memcpy(g.puzzles.wire.links,lume_wire_solution,sizeof lume_wire_solution);
  if(p==LUME_PIANO)for(int i=0;i<5;i++)piano_note(&g.puzzles.piano,lume_piano_solution[i]);
  if(p==LUME_SOLAR)memset(g.puzzles.solar.turns,0,sizeof g.puzzles.solar.turns);
  if(p==LUME_MAP)map_select(&g.puzzles.map,4,4);
  assert(lume_submit(&g.puzzles,p));
 }
 assert(g.puzzles.completed[p]);lume_game_puzzle_result(&g);assert(g.puzzle==-1);
}
static void checkpoint(void){
 assert(lume_game_save(&g,"route.sav"));LumeGame loaded;assert(lume_game_load(&loaded,"route.sav"));g=loaded;
}
static void home(void){
 g.clue=-1; /* Closing the popup, as the native B/close handler does. */
 if(g.scene==GARDEN)act(GO_HOUSE);
 if(g.scene==ROOF)act(GO_LADDER);
 if(g.scene==FRONT)act(OPEN_FRONT);
 else if(g.scene!=HALL)act(GO_HALL);
 assert(g.scene==HALL&&g.puzzle==-1);
}
static void run(int a,int b,int c,int d,int resume){
 lume_game_new(&g,123);act(GO_LADDER);act(OPEN_FRONT);solve();
 act(OPEN_CUPBOARD);solve();act(OPEN_CUPBOARD);act(TAKE_TOOLS);act(TAKE_WIRE);g.clue=-1;
 act(OPEN_KEYS);solve();if(resume)checkpoint();
 int order[]={a,b,c,d};
 for(int i=0;i<4;i++){
  home();
  if(order[i]==0){act(GO_POWER);g.selected=WIRE;act(OPEN_FUSE);assert(!g.fuse&&g.inventory&(1u<<KEYS));g.selected=KEYS;act(OPEN_FUSE);act(OPEN_FUSE);solve();}
  if(order[i]==1){act(GO_LADDER);act(OPEN_SOLAR);solve();}
  if(order[i]==2){act(GO_STAIRS);act(OPEN_MAP);solve();}
  if(order[i]==3){act(OPEN_PANTRY);solve();act(OPEN_PANTRY);
   unsigned before=g.inventory;act(TAKE_DYNAMO);act(TAKE_POLES);assert(g.inventory==before);
   g.selected=SPANNER;act(DISMANTLE_BIKE);act(TAKE_DYNAMO);act(DISMANTLE_BIKE);act(TAKE_POLES);
  }
  if(resume)checkpoint();
 }
 home();act(GO_LADDER);act(GO_LADDER);act(TAKE_BLADES);act(GO_GARDEN);
 int parts[]={POLES,DYNAMO,BLADES,WIRE};
 for(int i=0;i<4;i++){
  unsigned inventory=g.inventory;int stage=g.turbine;
  g.selected=KEYS;act(BUILD_TURBINE);assert(g.inventory==inventory&&g.turbine==stage);
  g.selected=parts[i];act(BUILD_TURBINE);assert(g.turbine==stage+1&&g.selected==-1);
  if(resume)checkpoint();
 }
 home();act(GO_POWER);act(OPEN_METER);act(PRESS_POWER);assert(g.ended&&lume_game_power_full(&g));runs++;
}
int main(void){
 for(int a=0;a<4;a++)for(int b=0;b<4;b++)for(int c=0;c<4;c++)for(int d=0;d<4;d++)
  if(a!=b&&a!=c&&a!=d&&b!=c&&b!=d&&c!=d)for(int resume=0;resume<2;resume++)run(a,b,c,d,resume);
 assert(runs==48);return 0;
}
'''
  native.ExceptionPathTests().native_check(program)
