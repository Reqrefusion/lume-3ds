"""Execute indexed depth I/O and title reprojection, with a bounded codec stub."""
from pathlib import Path
import os,subprocess,tempfile,unittest
from test_fence_flow import ROOT,compiler
class DepthStreamTests(unittest.TestCase):
 def test_seek_cache_invalid_data_and_slider(self):
  cc=compiler()
  if not cc:self.skipTest('Native C compiler unavailable')
  program=r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static int codec_reads;
int LZ4_decompress_safe(const char *src,char *dst,int n,int cap){codec_reads++;if(n!=cap||(uint8_t)src[0]==128)return -1;memcpy(dst,src,n);return n;}
#include "engine/assets/depth_stream.h"
#include "engine/render/disparity.h"
#include "game/lume/logo_depth.h"
#define CHECK(x) do{if(!(x)){printf("FAIL %d\n",__LINE__);return 1;}}while(0)
static uint8_t left[230400],right[230400],original[230400];
static void word(FILE *f,unsigned n){for(unsigned i=0;i<4;i++)fputc((n>>(8*i))&255,f);}
static void fixture(const char *path,bool corrupt){FILE *f=fopen(path,"wb");fwrite("TSF1",1,4,f);word(f,160);word(f,120);word(f,2);word(f,28);word(f,19228);word(f,38428);for(unsigned i=0;i<19200;i++)fputc(corrupt?128:4,f);for(unsigned i=0;i<19200;i++)fputc(252,f);fclose(f);}
int main(void){
 fixture("depth.tsf",false);LumeDepthStream stream={0};CHECK(lume_depth_open(&stream,"depth.tsf",2));
 const int8_t *map=lume_depth_frame(&stream,0);CHECK(map&&map[0]==4&&map[76799]==4);int reads=codec_reads;
 CHECK(lume_depth_frame(&stream,0)==map&&codec_reads==reads);CHECK(!lume_depth_frame(&stream,2));
 for(unsigned y=0;y<240;y++)for(unsigned x=0;x<320;x++)for(unsigned c=0;c<3;c++)left[(y*320+x)*3+c]=(uint8_t)x;
 memcpy(original,left,sizeof left);lume_puzzle_reproject_strength(left,right,map,0);CHECK(!memcmp(left,right,sizeof left));
 lume_puzzle_reproject_strength(left,right,map,256);CHECK(right[0]==left[3]&&right[319*3]==left[319*3]);CHECK(!memcmp(left,original,sizeof left));
 map=lume_depth_frame(&stream,1);CHECK(map&&map[0]==-4&&map[76799]==-4);
 lume_depth_close(&stream);CHECK(!stream.file&&!stream.map&&!lume_depth_frame(&stream,0));
 CHECK(!lume_depth_open(&stream,"depth.tsf",3));CHECK(!stream.file);
 fixture("bad.tsf",true);CHECK(lume_depth_open(&stream,"bad.tsf",2));CHECK(!lume_depth_frame(&stream,0));
 CHECK(lume_depth_frame(&stream,1)!=NULL);lume_depth_close(&stream);
 memcpy(right,original,sizeof right);lume_logo_ending_front(right,0);CHECK(!memcmp(right,original,sizeof right));
 lume_logo_ending_front(right,256);CHECK(right[18*960+102*3]==105);
 for(unsigned y=0;y<240;y++)for(unsigned x=0;x<320;x++)if(y<18||y>=62||x<102||x>=218)CHECK(!memcmp(right+(y*320+x)*3,original+(y*320+x)*3,3));
 memcpy(right,original,sizeof right);lume_logo_ending_front(right,128);CHECK(right[18*960+102*3]==104);
 lume_logo_title_front(left,right,256);CHECK(right[40*960+100*3]==103);CHECK(!memcmp(left,original,sizeof left));
 puts("PASS indexed depth: counts, seek/cache, malformed packet, recovery, slider zero/full, original RGB preserved");return 0;
}
"""
  work=ROOT/'work';work.mkdir(exist_ok=True)
  with tempfile.TemporaryDirectory(prefix='depth_stream_',dir=work) as folder:
   temp=Path(folder);(temp/'lz4.h').write_text('int LZ4_decompress_safe(const char*,char*,int,int);',encoding='utf-8')
   test=temp/'test.c';test.write_text(program,encoding='utf-8');exe=temp/('test.exe' if os.name=='nt' else 'test');path,env,msvc=cc
   source=ROOT/'source'
   args=([path,'/nologo','/std:c11','/utf-8','/O2','/MT','/I'+str(temp),'/I'+str(source),str(test),'/Fe:'+str(exe)] if msvc else [path,'-O2','-std=c11','-I'+str(temp),'-I'+str(source),str(test),'-o',str(exe)])
   result=subprocess.run(args,cwd=temp,env=env,capture_output=True,text=True);self.assertEqual(result.returncode,0,result.stdout+result.stderr)
   result=subprocess.run([str(exe)],cwd=temp,env=env,capture_output=True,text=True);self.assertEqual(result.returncode,0,result.stdout+result.stderr);print(result.stdout.strip())
if __name__=='__main__':unittest.main()
