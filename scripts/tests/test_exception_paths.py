"""Boundary inputs execute the actual native sidecar name builder."""
from pathlib import Path
import os,subprocess,tempfile,unittest
from test_fence_flow import ROOT,compiler,block

class ExceptionPathTests(unittest.TestCase):
 def native_check(self,program):
  cc=compiler()
  if not cc:self.skipTest('Native C compiler unavailable')
  self.run_native(program,cc)

 def test_audio_cleanup_is_repeatable(self):
  function=block((ROOT/'source/game/lume/audio.c').read_text(),'void lume_audio_quit(')
  program=r'''
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#define U64_MAX UINT64_MAX
#define MUSIC_BUFFERS 6
#define EFFECT_CHANNEL 9
#define EFFECT_VOICES 8
static bool music_hooked,ready=true,mpg_ready=true,music_fading=true,stop_worker;
static int music_apt_hook,music_lock,music_request;static unsigned effects_played;
static void *music_worker,*music=(void*)1,*note_data[8],*music_data[6];
static int stream[6],notes[8],effects[8];
static struct {void *data;} samples[2];
static unsigned frees,deleted,exits,dsp_exits;
static void aptUnhook(void *p){(void)p;}
static void LightLock_Lock(void *p){(void)p;}
static void LightLock_Unlock(void *p){(void)p;}
static void threadJoin(void *p,uint64_t t){(void)p;(void)t;}
static void threadFree(void *p){(void)p;}
static void mpg123_close(void *p){(void)p;}
static void mpg123_delete(void *p){(void)p;deleted++;}
static void music_drop_ram(void){}
static void ndspChnWaveBufClear(int c){(void)c;}
static void ndspExit(void){dsp_exits++;}
static void linearFree(void *p){(void)p;frees++;}
static void free_authored_events(void){}
static void lume_log(const char *f,...){(void)f;}
static void mpg123_exit(void){exits++;}
'''+function+r'''
int main(void){
 note_data[0]=(void*)2;music_data[0]=(void*)3;samples[0].data=(void*)4;
 lume_audio_quit();lume_audio_quit();
 if(frees!=3||deleted!=1||exits!=1||dsp_exits!=1||ready||mpg_ready||music||music_fading)return 1;
 return 0;
}
'''
  self.native_check(program)

 def test_raster_header_product_cannot_wrap(self):
  function=block((ROOT/'source/engine/assets/raster.h').read_text(),'static inline bool lume_asset_read(')
  program=r'''
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int decoded;
static int LZ4_decompress_safe(const char *a,char *b,int c,int d){(void)a;(void)b;(void)c;decoded++;return d;}
static bool lume_pixel_restore(void *p,unsigned w,unsigned h,unsigned c,unsigned m){(void)p;(void)w;(void)h;(void)c;(void)m;return true;}
'''+function+r'''
int main(void){
 /* 32769*32768*4 = 2^32+131072. Reject before decoder/output access. */
 unsigned char header[17]={'L','R','C','1',0,0,2,0,1,128,0,128,4,1,1,0,0};
 FILE *f=tmpfile();if(!f)return 1;
 if(fwrite(header,1,sizeof header,f)!=sizeof header)return 2;
 bool ok=lume_asset_read(f,NULL,131072);fclose(f);
 if(ok||decoded)return 3;
 return 0;
}
'''
  self.native_check(program)

 def test_sidecar_name_boundaries(self):
  cc=compiler()
  if not cc:self.skipTest('Native C compiler unavailable')
  source=(ROOT/'source/adapters/flash/timeline.c').read_text()
  function=block(source,'static bool cue_path(')
  program=r'''
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
'''+function+r'''
#define CHECK(x) do {if(!(x))return __LINE__;}while(0)
int main(void){
 struct {char out[160];unsigned guard;} result;result.guard=0x12345678;
 CHECK(cue_path(result.out,160,"romfs:/intro.lan",".sf2"));
 CHECK(!strcmp(result.out,"romfs:/intro.sf2"));
 CHECK(!cue_path(result.out,160,"folder.name/intro",".sfx"));
 CHECK(!cue_path(result.out,160,"folder.name\\intro",".sfx"));
 char path[200];
 for(unsigned n=1;n<190;n++){
  memset(path,'a',n);path[n]='.';path[n+1]='x';path[n+2]=0;
  bool ok=cue_path(result.out,160,path,".sf2");
  CHECK(ok==(n+5<=160));CHECK(result.guard==0x12345678);
  if(ok)CHECK(strlen(result.out)==n+4&&!strcmp(result.out+n,".sf2"));
 }
 CHECK(!cue_path(result.out,0,"x.lan",".sf2"));
 return 0;
}
'''
  self.run_native(program,cc)

 def run_native(self,program,cc):
  work=ROOT/'work';work.mkdir(exist_ok=True)
  with tempfile.TemporaryDirectory(prefix='exception_test_',dir=work) as folder:
   temp=Path(folder);test=temp/'test.c';test.write_text(program)
   exe=temp/('test.exe' if os.name=='nt' else 'test');path,env,msvc=cc
   args=([path,'/nologo','/std:c11','/MT',str(test),'/Fe:'+str(exe)] if msvc else [path,'-std=c11',str(test),'-o',str(exe)])
   build=subprocess.run(args,cwd=temp,env=env,capture_output=True,text=True)
   self.assertEqual(build.returncode,0,build.stdout+build.stderr)
   result=subprocess.run([str(exe)],cwd=temp,env=env,capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
