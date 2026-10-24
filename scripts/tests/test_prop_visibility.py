"""Compare real native prop drawing against source geometry across visibility states."""
from pathlib import Path
import os,sys,subprocess,tempfile,unittest,struct
import numpy as np
from PIL import Image
from test_fence_flow import ROOT,compiler,block
sys.path.insert(0,str(ROOT/'scripts/exe'))
from assemble import overlay_prop
from stereo_fields import raster
class PropVisibilityTests(unittest.TestCase):
 def test_world_and_navigation_match_source_visibility(self):
  cc=compiler()
  if not cc:self.skipTest('Native C compiler unavailable')
  native=ROOT/'source';rom=ROOT/'romfs'
  code=r"""
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "engine/render/colour.h"
#include "game/lume/props.h"
#undef fopen
FILE *fopen(const char*,const char*);
static const char *asset_root;
FILE *prop_test_fopen(const char *path,const char *mode){char real[1024];if(!strncmp(path,"romfs:/",7)){snprintf(real,sizeof real,"%s/%s",asset_root,path+7);path=real;}return fopen(path,mode);}
unsigned lume_stereo_strength(void){return 256;}
"""
  code+=block((ROOT/'source/engine/render/blit.c').read_text(encoding='utf-8'),'void lume_blit_affine(')
  code+=r"""
static uint8_t clean[230400],rgb[230400];
int main(int argc,char **argv){
 if(argc!=3)return 1;asset_root=argv[1];if(!lume_props_init())return 2;
 for(unsigned scene=0;scene<2;scene++){
  char file[1024];snprintf(file,sizeof file,"%s/clean%u.rgb",argv[2],scene);FILE *f=fopen(file,"rb");if(!f||fread(clean,1,sizeof clean,f)!=sizeof clean)return 3;fclose(f);
  for(int stage=1;stage<=5;stage++)for(int blades=0;blades<2;blades++)for(int map=0;map<2;map++)for(int solar=0;solar<2;solar++){
   memcpy(rgb,clean,sizeof rgb);if(!lume_props_world(rgb,scene,stage,blades!=0,map!=0,solar==0,0))return 4;
   snprintf(file,sizeof file,"%s/world_%u_%d_%d_%d_%d.rgb",argv[2],scene,stage,blades,map,solar);f=fopen(file,"wb");if(!f||fwrite(rgb,1,sizeof rgb,f)!=sizeof rgb)return 5;fclose(f);
  }
 }
 if(lume_props_world(rgb,2,1,false,true,true,0))return 6;
 if(!lume_props_open("nav_0_1",120))return 7;
 for(int map=0;map<2;map++)for(int solar=0;solar<2;solar++){
  memset(rgb,0,sizeof rgb);lume_props_set_visibility(map!=0,solar==0);lume_props_frame(rgb,119,5,false,0);
  char file[1024];snprintf(file,sizeof file,"%s/nav_%d_%d.rgb",argv[2],map,solar);FILE *f=fopen(file,"wb");if(!f)return 8;fwrite(rgb,1,sizeof rgb,f);fclose(f);
 }
 lume_props_quit();puts("PASS native world visibility: both scenes, five stages, fence states and navigation");return 0;
}
"""
  with tempfile.TemporaryDirectory(prefix='prop_visibility_',dir=ROOT/'work') as folder:
   temp=Path(folder)
   for i,name in enumerate(['garden','roof']):(temp/f'clean{i}.rgb').write_bytes(raster(rom/f'world_clean_{name}.rgb').tobytes())
   source=temp/'test.c';source.write_text(code,encoding='utf-8');exe=temp/('test.exe' if os.name=='nt' else 'test');path,env,msvc=cc;files=[source,native/'game/lume/props.c']
   args=([path,'/nologo','/std:c11','/utf-8','/O2','/MT','/Dfopen=prop_test_fopen','/I'+str(native),*map(str,files),'/Fe:'+str(exe)] if msvc else [path,'-O2','-std=c11','-Dfopen=prop_test_fopen','-I'+str(native),*map(str,files),'-lm','-o',str(exe)])
   p=subprocess.run(args,cwd=temp,env=env,capture_output=True,text=True);self.assertEqual(p.returncode,0,p.stdout+p.stderr)
   p=subprocess.run([str(exe),str(rom),str(temp)],cwd=temp,env=env,capture_output=True,text=True);self.assertEqual(p.returncode,0,p.stdout+p.stderr)
   matrices=struct.unpack_from('<36f',(rom/'world_props.trf').read_bytes(),12);checked=0;worst=0
   for scene,name in enumerate(['garden','roof']):
    clean=Image.fromarray(raster(rom/f'world_clean_{name}.rgb'))
    for stage in range(1,6):
     for blades in range(2):
      for map_seen in range(2):
       for solar_done in range(2):
        expected=clean
        for i,asset,visible in [(0,f'turbine{stage}',map_seen),(1,f'fence{blades}',True),(2,'solar',not solar_done)]:
         if visible:expected=overlay_prop(expected,rom/f'prop_{asset}.bin',matrices[scene*18+i*6:scene*18+i*6+6])
        actual=np.frombuffer((temp/f'world_{scene}_{stage}_{blades}_{map_seen}_{solar_done}.rgb').read_bytes(),np.uint8).reshape(240,320,3)
        error=float(np.abs(actual.astype(np.int16)-np.asarray(expected).astype(np.int16)).mean());worst=max(worst,error);self.assertLess(error,.5);checked+=1
   transforms=struct.unpack_from('<18f',(rom/'nav_0_1.trf').read_bytes(),12+119*72)
   for map_seen in range(2):
    for solar_done in range(2):
     expected=Image.new('RGB',(320,240))
     for i,asset,visible in [(0,'turbine5',map_seen),(1,'fence0',True),(2,'solar',not solar_done)]:
      if visible:expected=overlay_prop(expected,rom/f'prop_{asset}.bin',transforms[i*6:i*6+6])
     actual=np.frombuffer((temp/f'nav_{map_seen}_{solar_done}.rgb').read_bytes(),np.uint8).reshape(240,320,3)
     self.assertLess(float(np.abs(actual.astype(np.int16)-np.asarray(expected).astype(np.int16)).mean()),.5)
   print(p.stdout.strip(),f'({checked} world states, 4 navigation states; worst RGB MAE={worst:.4f})')
if __name__=='__main__':unittest.main()
