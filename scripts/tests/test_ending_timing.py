"""Source-timed outro reader: no pause, no stale paper in gaps, bounded pages."""
from pathlib import Path
import unittest,os,subprocess,tempfile
from test_fence_flow import ROOT,compiler,block
class EndingTimingTests(unittest.TestCase):
 def test_actual_sync_uses_original_intervals_without_changing_film_clock(self):
  cc=compiler()
  if not cc:self.skipTest('Native C compiler unavailable')
  rows=[line.split() for line in (ROOT/'romfs/ending_reading.tsv').read_text().splitlines()]
  self.assertEqual(len(rows),7)
  cues=','.join('{'+a+','+b+',"'+label+'"}' for a,b,label in rows)
  code=r"""
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "features/reading/timeline.h"
#include "engine/core/timeline_clock.h"
typedef struct {bool timed,presented;unsigned pages,page,serial;} Reader;
static Reader reading;static bool cinema_reading,reading_replace,available=true;
static unsigned visual_epoch,opens,closes;static int ending_cue_current=-1;static unsigned ending_cue_count=7;
static uint64_t clip_start=1000;
static void lume_read_close(Reader *r){r->timed=false;closes++;}
static bool lume_read_open(Reader *r,const char *s){(void)s;opens++;r->page=0;r->timed=false;return available;}
static void lume_read_layout(Reader *r,unsigned width){r->pages=2;if(width!=320)r->pages=999;}
static void lume_log(const char *s,...){(void)s;}
static LumeTimedReadingCue ending_cues[7]={CUES};
#define CHECK(x) do{if(!(x)){printf("FAIL %d\n",__LINE__);return 1;}}while(0)
""".replace('CUES',cues)
  code+=block((ROOT/'source/main.c').read_text(encoding='utf-8'),'static void ending_reading_sync(')
  code+=r"""
int main(void){
 ending_reading_sync(0);CHECK(!cinema_reading&&opens==0);
 for(unsigned i=0;i<7;i++){
  const LumeTimedReadingCue *cue=&ending_cues[i];unsigned before=opens;
  ending_reading_sync(cue->first-1);CHECK(!cinema_reading);
  ending_reading_sync(cue->first);CHECK(cinema_reading&&reading.timed&&!reading_replace&&reading.pages==2&&reading.page==0&&opens==before+1);
  ending_reading_sync(cue->first+1);CHECK(opens==before+1);
  ending_reading_sync(cue->last);CHECK(cinema_reading&&reading.page==1);
  ending_reading_sync(cue->last+1);CHECK(!cinema_reading&&!reading.timed);
  CHECK(clip_start==1000);
 }
 // A late display selects its current cue, not an earlier entrance or pause.
 unsigned frame=lume_animation_clock_frame(1000+900*40,clip_start,25,1908,721);
 CHECK(frame==900);ending_reading_sync(frame);CHECK(ending_cue_current==2&&cinema_reading);
 ending_reading_sync(1500);CHECK(!cinema_reading);
 available=false;unsigned before=opens;ending_reading_sync(790);ending_reading_sync(791);CHECK(!cinema_reading&&opens==before+1&&clip_start==1000);
 CHECK(lume_read_cue_page(&ending_cues[0],670,0)==0);
 CHECK(lume_read_cue_page(&ending_cues[0],721,128)==127);
 puts("PASS original seven outro intervals, automatic pages, gap/skip/failure cleanup and uninterrupted clock");return 0;
}
"""
  with tempfile.TemporaryDirectory(prefix='ending_timing_',dir=ROOT/'work') as folder:
   temp=Path(folder);source=temp/'test.c';source.write_text(code,encoding='utf-8');exe=temp/('test.exe' if os.name=='nt' else 'test');path,env,msvc=cc;inc=ROOT/'source'
   args=([path,'/nologo','/std:c11','/utf-8','/O2','/MT','/I'+str(inc),str(source),'/Fe:'+str(exe)] if msvc else [path,'-O2','-std=c11','-I'+str(inc),str(source),'-o',str(exe)])
   p=subprocess.run(args,cwd=temp,env=env,capture_output=True,text=True);self.assertEqual(p.returncode,0,p.stdout+p.stderr)
   p=subprocess.run([str(exe)],cwd=temp,env=env,capture_output=True,text=True);self.assertEqual(p.returncode,0,p.stdout+p.stderr);print(p.stdout.strip())
if __name__=='__main__':unittest.main()
