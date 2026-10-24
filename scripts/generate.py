"""Regenerate UI/languages only; never overwrite sources or convert movies."""
from pathlib import Path
import argparse,json,shutil,sys,os,re,struct,hashlib
sys.path.insert(0,str(Path(__file__).resolve().parent))
import toolchain as cv
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'build'))
from manifest import commit,sha,dump
HERE=Path(__file__).resolve().parent

def run(args):
 root=Path(args.root).resolve()
 from check_languages import check
 check(root/'languages')
 inputs=[HERE/'generate.py',HERE/'toolchain.py',*sorted((HERE/'java').glob('*.class')),*sorted(p for p in (root/'languages').glob('*') if p.is_file()),HERE/'check_languages.py']
 signature=hashlib.sha256((''.join(sha(p) for p in inputs)+cv.SWF_SHA).encode()).hexdigest()
 state=root/'tools/ui-state.json'
 if state.is_file() and not getattr(args,'force',False):
  try:saved=json.loads(state.read_text(encoding="utf-8"))
  except (ValueError,OSError):saved={}
  rom=root/'romfs'
  if saved.get('signature')==signature and isinstance(saved.get('outputs'),dict) and all((rom/n).is_file() and sha(rom/n)==h for n,h in saved['outputs'].items()):
   print('Languages, fonts and menus are current; regeneration skipped.');return
 folder=root/'languages';packs=[];all_chars=set(chr(i) for i in range(32,127))
 for p in sorted(folder.glob('*.json')):
  pack=json.loads(p.read_text(encoding='utf-8'));ident=pack.get('id','');name=pack.get('name','')
  if not re.fullmatch('[a-z0-9_]{1,15}',ident) or ident!=p.stem:raise ValueError('Language identifier/filename mismatch: '+p.name)
  if not name or len(name.encode('utf-8'))>63 or any(c in name for c in '\r\n\t'):raise ValueError('Invalid language name: '+ident)
  for group in ['ui','text']:
   if not isinstance(pack.get(group),dict):raise ValueError('ui/text dictionary required: '+ident)
   if group=='ui' and len(pack[group])>64:raise ValueError('En fazla64 arayuz anahtari')
   for key,value in pack[group].items():
    if not re.fullmatch('[A-Za-z][A-Za-z0-9_]{0,63}',key) or not isinstance(value,str):raise ValueError('Invalid language text: '+ident+'/'+key)
    if group=='ui' and (len(key)>31 or len(value)>159 or any(c in value for c in '\r\n\t')):raise ValueError('Arayuz metni cok uzun veya cok satirli: '+key)
    if len(value.encode('utf-8'))>8191 or any((ord(c)<32 and c not in '\n\r\t') or ord(c)==127 for c in value):raise ValueError('Metin okuyucu siniri: '+key)
    all_chars.update(c for c in value if ord(c)>=32)
  all_chars.update(name);all_chars.update(pack.get("characters",""));packs.append(pack)
 if not packs or len(packs)>16 or 'en' not in [p['id'] for p in packs]:raise ValueError('en.json ve en fazla16 dil bekleniyor')
 if len(all_chars)>224:raise ValueError('Bu atlas en fazla224 farkli karakter destekler; kullanilan: '+str(len(all_chars)))
 work=root/'tools/settings/work';work.mkdir(parents=True,exist_ok=True);chars=work/'font-chars.txt';chars.write_text(''.join(sorted(all_chars)),encoding='utf-8')
 cache=root/'tools/detail';swf=cv.source_swf(root,cache,args);java,jar,_=cv.tools(cache,args,need_ffmpeg=False)
 cp=os.pathsep.join([str(HERE/'java'),str(jar),str(jar.parent/'lib/*')])
 print('Generating original text, font, paper, inventory, buttons and language packs. Video is not converted.',flush=True)
 cv.run_logged([str(java),'-Xmx768m','-Djava.awt.headless=true','-cp',cp,'SettingsAssets',str(swf),str(chars)],work)
 raw=work/'project/romfs';original={p.stem:p.read_text(encoding='utf-8').rstrip('\n') for p in (raw/'reading').glob('*.txt')}
 catalog=set((root/'languages/text-keys.txt').read_text(encoding='utf-8').splitlines())
 if set(original)!=catalog:raise ValueError('Original text catalogue changed; update every full translation before generating assets.')
 for pack in packs:
  unknown=set(pack['text'])-set(original)
  if unknown:raise ValueError('Original text key not found: '+','.join(sorted(unknown)))
 generated=work/'compiled';shutil.rmtree(generated,ignore_errors=True);generated.mkdir()
 (generated/'lang').mkdir();(generated/'lang/languages.txt').write_text(''.join(p['id']+'\t'+p['name']+'\n' for p in packs),encoding='utf-8')
 font=(raw/'locale_font.bin').read_bytes();n=struct.unpack_from('<I',font,4)[0]
 if font[:4]!=b'LUF1' or not 95<=n<=224 or len(font)!=8+n*244:raise ValueError('Invalid language font header')
 for pack in packs:
  dest=generated/'lang'/pack['id'];dest.mkdir();(dest/'font.bin').write_bytes(font)
  (dest/'ui.txt').write_text(''.join(k+'\t'+v+'\n' for k,v in pack['ui'].items() if v.strip()),encoding='utf-8')
  text=original if pack['id']=='en' else {k:v for k,v in pack['text'].items() if v.strip()}
  text=dict(text,**pack['text']) if pack['id']=='en' else text
  for key,value in text.items():(dest/(key+'.txt')).write_text(value+'\n',encoding='utf-8')
 # Fallback original English conversations/paper work even on an earlier kit.
 for p in (raw/'reading').iterdir():
  cv.native_valid(p);target=generated/'detail/reading'/p.name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,target)
 for name in ['art.bin','font.bin']:
  p=raw/'native_menu'/name;cv.native_valid(p);target=generated/'detail/native_menu'/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,target)
 cv.run_logged([str(java),'-Xmx512m','-Djava.awt.headless=true','-cp',cp,'HoverArt',str(swf)],work)
 shutil.copyfile(raw/'hover.bin',generated/'hover.bin')
 shutil.copyfile(raw/'ending_reading.tsv',generated/'ending_reading.tsv')
 commit(root,generated)
 cv.dump(root/'logs/DIL_PAKETLERI.json',{'original_labels':len(original),'languages':[p['id'] for p in packs],'glyphs':n,'videos_converted':False})
 dump(state,{'signature':signature,'outputs':{p.relative_to(generated).as_posix():sha(p) for p in generated.rglob('*') if p.is_file()}})

def main():
 p=argparse.ArgumentParser();p.add_argument('--root',required=True);p.add_argument('--force',action='store_true')
 for name in ['exe','java','ffdec','ffmpeg']:p.add_argument('--'+name)
 a=p.parse_args();run(a)
if __name__=='__main__':main()
