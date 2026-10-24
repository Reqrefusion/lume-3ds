"""Execute the real colour selection routine with deterministic converter timings."""
from pathlib import Path
import os,subprocess,tempfile,unittest
from test_fence_flow import ROOT,compiler,block
class ColourChoiceTests(unittest.TestCase):
 def test_warm_timings_fallback_and_colour_rejection(self):
  cc=compiler()
  if not cc:self.skipTest('Native C compiler unavailable')
  implementation=block((ROOT/'source/engine/codecs/mpeg_reader.h').read_text(encoding='utf-8'),'static void lume_mpeg_colour(')
  program=r"""
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define LUME_MEDIA_TOP 13
#define LUME_MEDIA_BOTTOM 226
#define LUME_MPEG_RGB_BYTES (320u*240u*3u)
#define CHECK(x) do{if(!(x)){printf("FAIL %d\n",__LINE__);return 1;}}while(0)
typedef struct {bool checked,disabled,direct;const char *failure;int result;} LumeY2R;
typedef struct {int unused;} plm_frame_t;
typedef struct {struct {uint64_t colour_hw_ticks,colour_sw_ticks;unsigned colour_samples;const char *y2r_failure;int y2r_result;uint64_t y2r_mae_milli;unsigned y2r_frames,y2r_direct_frames,y2r_fallbacks;} stats;} LumeMovie;
typedef struct {LumeY2R y2r;unsigned colour_samples;uint64_t colour_hw_ticks,colour_sw_ticks;bool colour_selected_software;uint8_t rgb[LUME_MPEG_RGB_BYTES];} LumeMpegState;
static uint64_t ticks;static unsigned hw_cost=50,sw_cost=30,hardware_calls,software_calls;static bool bad;
static uint64_t lume_mpeg_ticks(void){return ticks;}
static bool lume_y2r_frame(LumeY2R *y,plm_frame_t *p,uint8_t *rgb){
 (void)p;if(y->disabled)return false;hardware_calls++;ticks+=y->checked?hw_cost:9000;
 memset(rgb,bad?200:91,LUME_MPEG_RGB_BYTES);y->direct=true;return true;
}
static void lume_mpeg_rgb(plm_frame_t *p,uint8_t *rgb){(void)p;software_calls++;ticks+=sw_cost;memset(rgb,90,LUME_MPEG_RGB_BYTES);}
static void lume_y2r_close(LumeY2R *y){(void)y;}
"""+implementation+r"""
static LumeMpegState s;static LumeMovie movie;
int main(void){
 plm_frame_t picture={0};
 lume_mpeg_colour(&movie,&picture,&s);
 CHECK(s.y2r.checked&&s.colour_samples==0&&!s.y2r.disabled);
 for(unsigned i=0;i<3;i++)lume_mpeg_colour(&movie,&picture,&s);
 CHECK(s.colour_samples==3&&s.colour_selected_software&&s.y2r.disabled);
 CHECK(s.colour_hw_ticks==150&&s.colour_sw_ticks==90);
 CHECK(!strcmp(movie.stats.y2r_failure,"software_faster")&&movie.stats.y2r_result==0);
 CHECK(s.rgb[10000]==90&&software_calls==4&&hardware_calls==4);
 lume_mpeg_colour(&movie,&picture,&s);
 CHECK(hardware_calls==4&&software_calls==5&&s.rgb[10000]==90);
 memset(&s,0,sizeof s);hw_cost=20;sw_cost=30;
 for(unsigned i=0;i<7;i++)lume_mpeg_colour(&movie,&picture,&s);
 CHECK(s.colour_samples==3&&!s.y2r.disabled&&s.rgb[10000]==91);
 memset(&s,0,sizeof s);hw_cost=33;sw_cost=30;
 for(unsigned i=0;i<4;i++)lume_mpeg_colour(&movie,&picture,&s);
 CHECK(!s.y2r.disabled); /* Small differences do not change the path. */
 memset(&s,0,sizeof s);bad=true;
 lume_mpeg_colour(&movie,&picture,&s);
 CHECK(s.y2r.disabled&&!s.colour_selected_software&&s.rgb[10000]==90);
 CHECK(!strcmp(movie.stats.y2r_failure,"colour_check")&&movie.stats.y2r_result==-1);
 puts("PASS actual colour routine: initialization excluded, bounded warm sample, faster path, hysteresis, quality rejection");return 0;
}
"""
  work=ROOT/'work';work.mkdir(exist_ok=True)
  with tempfile.TemporaryDirectory(prefix='colour_choice_',dir=work) as folder:
   temp=Path(folder);test=temp/'test.c';test.write_text(program,encoding='utf-8')
   exe=temp/('test.exe' if os.name=='nt' else 'test');path,env,msvc=cc
   args=([path,'/nologo','/std:c11','/utf-8','/O2','/MT',str(test),'/Fe:'+str(exe)] if msvc else [path,'-O2','-std=c11',str(test),'-o',str(exe)])
   result=subprocess.run(args,cwd=temp,env=env,capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
   result=subprocess.run([str(exe)],cwd=temp,env=env,capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
   print(result.stdout.strip())
if __name__=='__main__':unittest.main()
