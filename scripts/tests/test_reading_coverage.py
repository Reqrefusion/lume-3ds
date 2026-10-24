"""Use the actual reader for every authored speech label and audit stopped film children."""
from pathlib import Path
import os,sys,subprocess,tempfile,unittest
from test_fence_flow import ROOT,compiler
sys.path.insert(0,str(ROOT/'scripts/exe'))
from original_swf import Original,placement
from recover import actions
class ReadingCoverageTests(unittest.TestCase):
 def test_cinematic_internal_stops_have_an_explicit_state_policy(self):
  original=Original(ROOT/'tools/exe-production/recovered/Lume.swf');states=original.states(988);multi=set()
  for cid,events in original.timelines.items():
   last=max((fr for fr,t,x in events if t==1),default=0)
   if any(t==12 and fr<last and any(a['op']=='Stop' for a in actions(x)) for fr,t,x in events):multi.add(cid)
  def visit(cid,seen):
   if cid in seen:return
   seen.add(cid)
   for _,tag,data in original.timelines.get(cid,[]):
    if tag in (26,70):
     child=placement(tag,data).get('character')
     if child is not None:visit(child,seen)
  for first,last in [(1752,2536),(2538,4445)]:
   seen=set()
   for fr in range(first,last+1):
    for e in states[fr].values():visit(e['character'],seen)
   self.assertEqual(seen&multi,{208},'New stateful cinematic child needs an explicit source policy')
 def test_all_authored_speech_uses_actual_readable_assets(self):
  cc=compiler()
  if not cc:self.skipTest('Native C compiler unavailable')
  code=r"""
#include "features/reading/bubble.h"
LumeLocale lume_locale;
static const char *root;
unsigned lume_ui_fallback_alpha(unsigned ch,unsigned pixel){(void)ch;(void)pixel;return 0;}
bool lume_ui_user_path(char *out,size_t bytes,const char *relative){(void)out;(void)bytes;(void)relative;return false;}
FILE *lume_ui_asset_open(const char *relative){char path[1024];snprintf(path,sizeof path,"%s/%s",root,relative);return fopen(path,"rb");}
static LumeReading reading;
int main(int argc,char **argv){
 if(argc!=2)return 1;root=argv[1];FILE *f=lume_ui_asset_open("speech_labels.txt");if(!f)return 2;
 char label[128];unsigned text=0,empty=0;
 while(fgets(label,sizeof label,f)){
  label[strcspn(label,"\r\n")]=0;
  bool blank=!strcmp(label,"static")||!strcmp(label,"template")||!strcmp(label,"puzzleIncomplete");
  bool ready=lume_read_open(&reading,label);
  if(blank){if(ready)return 3;empty++;continue;}
  if(!ready||!reading.font_ready){printf("Unreadable %s\n",label);return 4;}
  for(unsigned w=320;w<=400;w+=80){lume_read_draw(&reading,w);if(!reading.pages||!lume_read_paper(&reading,w))return 5;}
  text++;
 }
 fclose(f);lume_read_release(&reading);
 if(text!=57||empty!=3)return 6;
 puts("PASS all 57 speech labels: actual text/font/paper, both screen widths; three intentionally empty effect labels");return 0;
}
"""
  with tempfile.TemporaryDirectory(prefix='reader_coverage_',dir=ROOT/'work') as folder:
   temp=Path(folder);source=temp/'test.c';source.write_text(code,encoding='utf-8');exe=temp/('test.exe' if os.name=='nt' else 'test');path,env,msvc=cc;inc=ROOT/'source'
   args=([path,'/nologo','/std:c11','/utf-8','/O2','/MT','/I'+str(inc),str(source),'/Fe:'+str(exe)] if msvc else [path,'-O2','-std=c11','-I'+str(inc),str(source),'-o',str(exe)])
   p=subprocess.run(args,cwd=temp,env=env,capture_output=True,text=True);self.assertEqual(p.returncode,0,p.stdout+p.stderr)
   p=subprocess.run([str(exe),str(ROOT/'romfs')],cwd=temp,env=env,capture_output=True,text=True);self.assertEqual(p.returncode,0,p.stdout+p.stderr);print(p.stdout.strip())
if __name__=='__main__':unittest.main()
