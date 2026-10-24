"""Execute source-derived power and turbine rules against actual game.c."""
from pathlib import Path
import os,subprocess,tempfile,unittest
from test_fence_flow import ROOT,compiler

class GameRuleTests(unittest.TestCase):
 def test_power_matrix_and_turbine_sequence(self):
  cc=compiler()
  if not cc:self.skipTest('Native C compiler unavailable')
  source=ROOT/'source'
  menu=(source/'game/lume/native_menu.h').read_text(encoding='utf-8-sig')
  menu=menu[menu.index('enum { LUME_MENU_CLOSED'):menu.index('static inline void lume_menu_box')]
  program=r'''
#include "game/lume/game.h"
#include "game/lume/clue_views.h"
#include <string.h>
#include <stdio.h>
#define CHECK(x) do {if(!(x)){printf("FAIL line %d\n",__LINE__);return 1;}}while(0)
MENU_INPUT_IMPLEMENTATION
int main(void){
 LumeGame g;char variant[64];
 for(int bits=0;bits<16;bits++){
  lume_game_new(&g,123);g.fuse=(bits&1)!=0;
  g.puzzles.completed[LUME_WIRES]=(bits&2)!=0;
  g.puzzles.completed[LUME_SOLAR]=(bits&4)!=0;
  g.turbine=(bits&8)?LUME_TURBINE_CONNECTED:LUME_TURBINE_EMPTY;
  unsigned level=(bits&1)&&(bits&2)?!!(bits&4)+!!(bits&8):0;
  CHECK(lume_game_power_level(&g)==level);
  CHECK(lume_game_power_full(&g)==(level==2));
  g.scene=POWER;g.clue=8;
  const char *assets[]={"meter0","meter1","meter2"};
  CHECK(!strcmp(lume_clue_asset(&g,variant,sizeof variant),assets[level]));
  CHECK(lume_game_action(&g,PRESS_POWER));CHECK(g.ended==(level==2));
 }
 lume_game_new(&g,123);g.scene=GARDEN;g.puzzles.completed[LUME_MAP]=true;
 int parts[]={POLES,DYNAMO,BLADES,WIRE};
 unsigned all=(1u<<POLES)|(1u<<DYNAMO)|(1u<<BLADES)|(1u<<WIRE);
 g.inventory=all;
 for(int i=0;i<4;i++){
  CHECK(lume_game_turbine_required_item(&g)==parts[i]);
  int stage=g.turbine;unsigned inventory=g.inventory;
  g.selected=SPANNER;CHECK(lume_game_action(&g,BUILD_TURBINE));
  CHECK(g.turbine==stage&&g.inventory==inventory);
  g.selected=parts[i];CHECK(lume_game_action(&g,BUILD_TURBINE));
  CHECK(g.turbine==stage+1&&g.selected==-1&&g.inventory==(inventory&~(1u<<parts[i])));
 }
 CHECK(g.turbine==LUME_TURBINE_CONNECTED&&g.inventory==0);
 CHECK(lume_game_turbine_required_item(&g)==-1);
 CHECK(lume_game_action(&g,BUILD_TURBINE));CHECK(g.turbine==LUME_TURBINE_CONNECTED);
 for(unsigned inventory=0;inventory<(1u<<LUME_ITEM_COUNT);inventory++)for(int stage=1;stage<=5;stage++){
  lume_game_new(&g,123);g.inventory=inventory;g.turbine=stage;g.clue=9;
  unsigned cupboard=((inventory&(1u<<SPANNER))?1u:0u)|((inventory&(1u<<WIRE))||stage>=LUME_TURBINE_CONNECTED?2u:0u);
  unsigned pantry=((inventory&(1u<<DYNAMO))||stage>=LUME_TURBINE_DYNAMO?1u:0u)|((inventory&(1u<<POLES))||stage>=LUME_TURBINE_POLES?2u:0u)|((inventory&(1u<<BINOCULARS))?4u:0u);
  CHECK(lume_game_cupboard_mask(&g)==cupboard&&lume_game_pantry_mask(&g)==pantry);
  CHECK(lume_clue_cache_key(&g)==9+(int)cupboard*16);
 }

 /* Every ownership mask and selected cursor round-trips through actual save/load.
    Exercise the real inventory menu by touch and A, including the keys slot. */
 for(unsigned inv=0;inv<128;inv++)for(int selected=-1;selected<7;selected++){
  lume_game_new(&g,123);g.inventory=inv;g.selected=selected;
  CHECK(lume_game_save(&g,"inventory.sav"));LumeGame loaded;
  CHECK(lume_game_load(&loaded,"inventory.sav"));
  CHECK(loaded.inventory==inv);
  CHECK(loaded.selected==((selected>=0&&(inv&(1u<<selected)))?selected:-1));
  for(unsigned slot=0;slot<7;slot++){
   LumeNativeMenu menu={0};g=loaded;g.selected=-1;
   lume_menu_input(&menu,&g,LM_SELECT,0,0,true);
   CHECK(menu.mode==LUME_MENU_INVENTORY);
   lume_menu_input(&menu,&g,LM_TOUCH,12+(slot%4)*76,48+(slot/4)*84,true);
   if(lume_menu_owned(&g,slot)){
    CHECK(menu.mode==LUME_MENU_CLOSED&&g.selected==lume_menu_items[slot]);
    lume_menu_input(&menu,&g,LM_SELECT,0,0,true);
    CHECK(menu.focus==slot);
    lume_menu_input(&menu,&g,LM_OK,0,0,true);
    CHECK(g.selected==-1&&menu.mode==LUME_MENU_CLOSED);
   }else CHECK(menu.mode==LUME_MENU_INVENTORY&&g.selected==-1);
  }
 }
 lume_game_new(&g,123);g.puzzle=LUME_KEYS;
 lume_game_puzzle_result(&g);CHECK(g.inventory==0&&g.puzzle==LUME_KEYS);
 g.puzzles.completed[LUME_KEYS]=true;
 lume_game_puzzle_result(&g);CHECK(g.inventory==(1u<<KEYS)&&g.selected==-1&&g.puzzle==-1);
 g.scene=POWER;g.selected=KEYS;
 CHECK(lume_game_action(&g,OPEN_FUSE));
 CHECK(g.fuse&&g.selected==-1&&g.inventory==(1u<<KEYS));

 /* Real on-disk recovery: incomplete/trailing/checksum-invalid primary files
    must use the verified backup without partially overwriting live state. */
 lume_game_new(&g,123);g.inventory=1u<<KEYS;
 CHECK(lume_game_save(&g,"recovery.sav"));
 g.inventory|=1u<<SPANNER;CHECK(lume_game_save(&g,"recovery.sav"));
 for(int damage=0;damage<3;damage++){
  uint32_t words[18];lume_game_snapshot(&g,words);
  if(damage==2)words[3]^=1;
  FILE *f=fopen("recovery.sav","wb");CHECK(f);
  CHECK(fwrite(words,1,damage==0?17:sizeof words,f)==(damage==0?17:sizeof words));
  if(damage==1)CHECK(fputc(0xff,f)!=EOF);
  CHECK(!fclose(f));LumeGame recovered;
  CHECK(lume_game_load(&recovered,"recovery.sav"));CHECK(recovered.inventory==(1u<<KEYS));
 }
 LumeGame recovered;CHECK(lume_game_load(&recovered,"recovery.sav"));
 recovered.inventory|=1u<<WIRE;CHECK(lume_game_save(&recovered,"recovery.sav"));
 FILE *f=fopen("recovery.sav","wb");CHECK(f&&!fclose(f));
 CHECK(lume_game_load(&recovered,"recovery.sav"));CHECK(recovered.inventory==(1u<<KEYS));
 f=fopen("recovery.sav.bak","wb");CHECK(f&&!fclose(f));
 LumeGame before=recovered;CHECK(!lume_game_load(&recovered,"recovery.sav"));
 CHECK(!memcmp(&before,&recovered,sizeof before));
 puts("PASS: save corruption/backup recovery; inventory save/menu/use matrix; 16 power states, ordered turbine consumption, 640 inventory/placement visibility masks");return 0;
}
'''
  program=program.replace('MENU_INPUT_IMPLEMENTATION',menu)
  work=ROOT/'work';work.mkdir(exist_ok=True)
  with tempfile.TemporaryDirectory(prefix='rules_test_',dir=work) as folder:
   temp=Path(folder);test=temp/'test.c';test.write_text(program)
   exe=temp/('test.exe' if os.name=='nt' else 'test')
   path,env,msvc=cc;files=[test,source/'game/lume/game.c',source/'game/lume/puzzles.c']
   args=([path,'/nologo','/std:c11','/utf-8','/MT','/I'+str(source),*map(str,files),'/Fe:'+str(exe)] if msvc else [path,'-std=c11','-I'+str(source),*map(str,files),'-o',str(exe)])
   result=subprocess.run(args,cwd=temp,env=env,capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
   result=subprocess.run([str(exe)],cwd=temp,env=env,capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
if __name__=='__main__':unittest.main()
