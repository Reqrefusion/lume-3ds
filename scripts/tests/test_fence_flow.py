"""Execute original fence speech/walk/explicit pickup with real game state and hits.

Only animation I/O is stubbed. The C handlers are extracted from main.c so a
position gate or accidental ground pickup cannot escape this regression.
"""
from pathlib import Path
import os,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[2]

def block(text,marker):
 start=text.index(marker);opening=text.index('{',start);depth=1;end=opening+1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]

def compiler():
 if os.name!='nt':
  cc=shutil.which('cc') or shutil.which('clang') or shutil.which('gcc')
  return (cc,os.environ.copy(),False) if cc else None
 finder=Path(os.environ.get('ProgramFiles(x86)',r'C:\Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
 if not finder.is_file():return None
 install=subprocess.check_output([str(finder),'-latest','-products','*','-property','installationPath'],text=True).strip()
 if not install:return None
 cl=subprocess.check_output([str(finder),'-latest','-products','*','-find',r'VC\Tools\MSVC\**\bin\Hostx64\x64\cl.exe'],text=True).strip().splitlines()
 if not cl:return None
 env=dict(os.environ)
 msvc=Path(cl[0]).parents[3]
 sdk=Path(os.environ.get('ProgramFiles(x86)',r'C:\Program Files (x86)'))/'Windows Kits/10'
 versions=sorted(p.name for p in (sdk/'Include').iterdir() if p.is_dir() and (sdk/'Lib'/p.name).is_dir())
 if not versions:return None
 version=versions[-1]
 env['INCLUDE']=os.pathsep.join(map(str,[msvc/'include',*[sdk/'Include'/version/n for n in ['ucrt','shared','um','winrt']]]))
 env['LIB']=os.pathsep.join(map(str,[msvc/'lib/x64',sdk/'Lib'/version/'ucrt/x64',sdk/'Lib'/version/'um/x64']))
 env['PATH']=str(Path(cl[0]).parent)+os.pathsep+env.get('PATH','')
 return cl[0],env,True

class FenceFlowTests(unittest.TestCase):
 def test_fence_feedback_sources_and_assets(self):
  source=ROOT/'docs/actionscript/DefineSprite_806'
  self.assertIn('takeFenceBlades()', (source/'frame_32/DoAction.as').read_text())
  self.assertIn('"bladesTaken"', (source/'frame_89/DoAction.as').read_text())
  for label in ['bladesThought','bladesTaken','puzzleComplete']:
   self.assertTrue((ROOT/f'romfs/lang/tr/{label}.txt').read_text(encoding='utf-8-sig').strip())
   self.assertTrue((ROOT/f'romfs/speech_{label}.rgba').is_file())
 def test_fence_speech_explicit_pickup_and_no_delayed_collection(self):
  cc=compiler()
  if not cc:self.skipTest('Native C compiler unavailable (MSVC or cc required)')
  source=ROOT/'source';text=(source/'main.c').read_text(encoding='utf-8-sig')
  request=block(text,'static void request_action(')
  finish=block(text,'static void finish_authored_move(')
  floor=block(text,'static void request_floor_move(')
  dispatch_source=block(text,'static void dispatch(')
  fence_case=dispatch_source[dispatch_source.index('case TAKE_BLADES:'):dispatch_source.index('case LOOK_LIGHTS:')]
  program=r'''
#include "game/lume/game.h"
#include "game/lume/interaction_flow.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static LumeGame game;
static LumeInteractionFlow interaction;
static int started,dispatched;
static const char *navigation_override,*last_line;
static void say(const char *line){last_line=line;}
static void lume_log(const char *format,...){(void)format;}
static bool play_clip(const char *name,int kind){(void)name;(void)kind;return false;}
static void lume_actor_restore(int scene){(void)scene;}
static void dispatch(LumeGame *g,int action){
 uint32_t old_inventory=g->inventory;int old_turbine=g->turbine;
 dispatched++;assert(lume_game_action(g,action));
 const char *line=NULL;
 switch(action){
'''+fence_case+'''
 default:break;
 }
 say(line);
}
static bool start_authored_move(const LumeGame *g,int destination,int action){
 assert(lume_motion_clip(g->scene,interaction.position,destination));
 started++;lume_interaction_begin(&interaction,g->scene,destination,action);return true;
}
'''+finish+'\n'+request+'\n'+floor+'''
static void click_roof_floor(int x){request_floor_move(&game,x);}
static void fresh(void){lume_game_new(&game,123);lume_interaction_reset(&interaction,game.scene);started=dispatched=0;last_line=NULL;}
int main(void){
 fresh();
 int fence=-1;
 for(int y=13;y<227&&fence<0;y++)for(int x=0;x<320;x++)
  if(lume_game_hotspot(&game,x,y)==TAKE_BLADES){fence=TAKE_BLADES;break;}
 assert(fence==TAKE_BLADES);
 request_action(&game,fence);
 assert(started==0&&!interaction.active&&interaction.action==-1);
 assert(last_line&&!strcmp(last_line,"bladesThought"));
 assert(dispatched==0&&game.inventory==0);
 /* A later/duplicate animation callback must not collect the earlier click. */
 finish_authored_move(&game,true);assert(game.inventory==0&&dispatched==0);
 assert(lume_game_hotspot(&game,280,200)<0);
 click_roof_floor(280);assert(started==1&&interaction.action==-1);
 request_action(&game,TAKE_BLADES);assert(game.inventory==0&&dispatched==0);
 finish_authored_move(&game,true);
 assert(interaction.position==1&&game.inventory==0&&dispatched==0);
 request_action(&game,TAKE_BLADES);
 assert(game.inventory==(1u<<BLADES)&&dispatched==1&&!interaction.active);
 assert(last_line&&!strcmp(last_line,"bladesTaken"));
 request_action(&game,TAKE_BLADES);
 assert(game.inventory==(1u<<BLADES)&&dispatched==1);
 assert(!strcmp(last_line,"puzzleComplete"));
 fresh();click_roof_floor(280);finish_authored_move(&game,false);
 assert(game.inventory==0&&dispatched==0);
 fresh();interaction.position=1;request_action(&game,GO_TOP_DOOR);
 assert(started==0&&game.scene==ROOF);
 fresh();interaction.position=1;game.top_door=true;
 assert(lume_motion_action_position(&game,1,GO_TOP_DOOR)==0);
 game.scene=FRONT;
 assert(lume_motion_action_position(&game,0,OPEN_SOLAR)==1);
 assert(lume_motion_action_position(&game,0,OPEN_FRONT)==2);
 game.scene=HALL;
 assert(lume_motion_action_position(&game,1,OPEN_KEYS)==0);
 assert(lume_motion_action_position(&game,0,GO_POWER)==1);
 assert(lume_motion_action_position(&game,0,GO_STAIRS)==2);
 fresh();game.turbine=4;interaction.position=0;
 request_action(&game,TAKE_BLADES);
 assert(started==0&&game.inventory==0);
 fresh();game.scene=POWER;game.inventory=1u<<KEYS;game.selected=KEYS;
 assert(lume_game_action(&game,OPEN_FUSE));
 assert(game.fuse&&game.selected==-1&&(game.inventory&(1u<<KEYS)));
 assert(lume_game_action(&game,OPEN_FUSE));assert(game.puzzle==LUME_WIRES);
 fresh();click_roof_floor(280);
 request_action(&game,GO_GARDEN);assert(started==1&&interaction.action==-1);
 finish_authored_move(&game,true);int calls=dispatched;
 finish_authored_move(&game,true);assert(dispatched==calls&&game.inventory==0);
 fresh();click_roof_floor(280);game.scene=HALL;
 finish_authored_move(&game,true);assert(game.inventory==0&&dispatched==0&&!interaction.active);
 fresh();request_action(&game,-1);request_action(&game,LUME_ACTION_COUNT);
 assert(started==0&&dispatched==0);
 fresh();game.scene=PANTRY;request_floor_move(&game,250);assert(started==0);
 /* The front door's two clips release OPEN_FRONT only after the return. */
 fresh();game.scene=FRONT;lume_interaction_reset(&interaction,FRONT);
 lume_interaction_begin(&interaction,FRONT,2,OPEN_FRONT);
 int action=123;
 assert(lume_interaction_finish(&interaction,FRONT,true,&action)==LUME_MOVE_TRY_DOOR&&action==-1);
 lume_interaction_begin(&interaction,FRONT,3,OPEN_FRONT);
 assert(lume_interaction_finish(&interaction,FRONT,true,&action)==LUME_MOVE_FRONT_RETURN);
 assert(action==OPEN_FRONT&&interaction.position==0&&!interaction.active);
 lume_interaction_reset(&interaction,HALL);lume_interaction_begin(&interaction,HALL,2,GO_STAIRS);
 assert(lume_interaction_finish(&interaction,HALL,true,&action)==LUME_MOVE_STAIRS);
 assert(action==GO_STAIRS&&interaction.position==1);
 puts("PASS: gameplay, single completion, scene binding, busy guard, door and stairs transactions");
 return 0;
}
'''
  work=ROOT/'work';work.mkdir(exist_ok=True)
  with tempfile.TemporaryDirectory(prefix='fence_test_',dir=work) as folder:
   temp=Path(folder).resolve();self.assertTrue(temp.is_relative_to(work.resolve()))
   test=temp/'test.c';test.write_text(program,encoding='utf-8')
   exe=temp/('test.exe' if os.name=='nt' else 'test')
   files=[test,source/'game/lume/game.c',source/'game/lume/puzzles.c']
   compiler_path,env,msvc=cc
   args=([compiler_path,'/nologo','/std:c11','/utf-8','/w','/MT','/I'+str(source),*map(str,files),'/Fe:'+str(exe)] if msvc else [compiler_path,'-std=c11','-I'+str(source),*map(str,files),'-o',str(exe)])
   build=subprocess.run(args,cwd=temp,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
   self.assertEqual(build.returncode,0,build.stdout)
   result=subprocess.run([str(exe)],cwd=temp,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
   self.assertEqual(result.returncode,0,result.stdout)
   self.assertIn('PASS:',result.stdout)
if __name__=='__main__':unittest.main()
