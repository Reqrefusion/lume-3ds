"""Execute language selection and persisted settings with the native locale code."""
from pathlib import Path
import json,struct,subprocess,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts/tests'))
from test_fence_flow import compiler

class LocaleTests(unittest.TestCase):
 def test_native_language_selection_and_saved_turkish_text(self):
  cc=compiler()
  if not cc:self.skipTest('Host C compiler unavailable')
  with tempfile.TemporaryDirectory() as folder:
   root=Path(folder);(root/'lang').mkdir();(root/'lang/languages.txt').write_text('de\tDeutsch\nen\tEnglish\ntr\tTurkce\n')
   tr=json.loads((ROOT/'languages/tr.json').read_text(encoding='utf-8'))
   for ident in ['en','de','tr']:
    lang=root/'lang'/ident;lang.mkdir();(lang/'ui.txt').write_text('continue\tContinue\n')
    codes=list(range(32,127))+[231,287,305,350,351,252,246,304]
    (lang/'font.bin').write_bytes(b'LUF1'+struct.pack('<I',len(codes))+struct.pack('<'+'I'*len(codes),*codes)+bytes(len(codes)*240))
   (root/'lang/tr/bladesTaken.txt').write_text(tr['text']['bladesTaken'],encoding='utf-8')
   program=r'''
#include "features/locale/locale.h"
#include <assert.h>
LumeLocale lume_locale;
bool lume_ui_user_path(char *out,size_t cap,const char *relative){return snprintf(out,cap,"%s",relative)>0;}
FILE *lume_ui_asset_open(const char *name){return fopen(name,"rb");}
int main(void){
 lume_locale_init();assert(!strcmp(lume_locale.languages[lume_locale.selected].id,"en"));assert(lume_locale.font_ready);
 for(unsigned i=0;i<lume_locale.count;i++)if(!strcmp(lume_locale.languages[i].id,"tr"))lume_locale.selected=i;
 lume_locale_load();assert(lume_locale.font_ready);assert(lume_settings_save());
 FILE *f=lume_locale_file("bladesTaken.txt");assert(f);char text[1024]={0};assert(fread(text,1,sizeof text-1,f)>0);fclose(f);
 lume_locale.selected=0;lume_locale_init();assert(!strcmp(lume_locale.languages[lume_locale.selected].id,"tr"));
 f=lume_locale_file("bladesTaken.txt");assert(f);char loaded[1024]={0};fread(loaded,1,sizeof loaded-1,f);fclose(f);assert(!strcmp(text,loaded));
 char encoded[1024];lume_locale_encode(encoded,sizeof encoded,text);assert(encoded[0]);assert(lume_locale_char(305)!='?');return 0;
}
'''
   source=root/'test.c';source.write_text(program);binary=root/('test.exe' if cc[2] else 'test')
   command=[cc[0],'/nologo','/TC','/utf-8','/I'+str(ROOT/'source'),str(source),'/Fe:'+str(binary)] if cc[2] else [cc[0],'-std=c11','-I'+str(ROOT/'source'),str(source),'-o',str(binary)]
   result=subprocess.run(command,cwd=root,env=cc[1],capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stdout+result.stderr)
   result=subprocess.run([str(binary)],cwd=root,capture_output=True,text=True);self.assertEqual(result.returncode,0,result.stdout+result.stderr)
if __name__=='__main__':unittest.main()