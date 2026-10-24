"""Check complete translation coverage without extracting or converting game media."""
from pathlib import Path
import argparse,json,re,unicodedata
ROOT=Path(__file__).resolve().parents[1]
EMPTY_TEXT={'template','puzzleIncomplete'}
ACCENTS={'\u0300','\u0301','\u0302','\u0303','\u0306','\u0307','\u0308','\u0327'}

def check(folder):
 folder=Path(folder);keys=set((folder/'text-keys.txt').read_text(encoding='utf-8').splitlines())
 if not keys or not EMPTY_TEXT<=keys:raise ValueError('Missing authored text catalogue')
 packs=[];chars=set(map(chr,range(32,127)));ui_keys=set(json.loads((folder/'en.json').read_text(encoding='utf-8'))['ui'])
 for path in sorted(folder.glob('*.json')):
  pack=json.loads(path.read_text(encoding='utf-8'));ident=pack.get('id','')
  if ident!=path.stem or not re.fullmatch('[a-z0-9_]{1,15}',ident):raise ValueError('Invalid language identifier: '+path.name)
  if pack.get('translation_scope')!='full':raise ValueError('Incomplete translation scope: '+ident)
  name=pack.get('name','')
  if not name or len(name.encode('utf-8'))>63 or any(c in name for c in '\r\n\t'):raise ValueError('Invalid language name: '+ident)
  if set(pack.get('ui',{}))!=ui_keys:raise ValueError('Missing or unknown menu keys: '+ident)
  text=pack.get('text')
  if not isinstance(text,dict) or (ident!='en' and set(text)!=keys) or (ident=='en' and set(text)-keys):raise ValueError('Missing or unknown story/document keys: '+ident)
  for group in ['ui','text']:
   for key,value in pack[group].items():
    if not isinstance(value,str) or (not value.strip() and key not in EMPTY_TEXT):raise ValueError('Empty translation: '+ident+'/'+key)
    if key in EMPTY_TEXT and value:raise ValueError('Intentionally empty source label must stay empty: '+ident+'/'+key)
    if len(value.encode('utf-8'))>8191:raise ValueError('Text exceeds reader capacity: '+ident+'/'+key)
    if group=='ui' and (len(value)>159 or any(c in value for c in '\r\n\t')):raise ValueError('Menu exceeds one-line buffer: '+ident+'/'+key)
    if any((ord(c)<32 and c not in '\r\n\t') or ord(c)==127 for c in value):raise ValueError('Invalid control character: '+ident+'/'+key)
    chars.update(c for c in value if ord(c)>=32)
  chars.update(name);chars.update(pack.get('characters',''));packs.append(pack)
 if len(packs)>16 or not any(p['id']=='en' for p in packs):raise ValueError('English and at most 16 languages are required')
 if len(chars)>224:raise ValueError('Font atlas exceeds 224 glyphs')
 for char in chars:
  parts=unicodedata.normalize('NFD',char)
  if 32<=ord(parts[0])<=126 and any(unicodedata.combining(c) and c not in ACCENTS for c in parts[1:]):raise ValueError('Unsupported font accent: '+repr(char))
 return {'languages':[p['id'] for p in packs],'authored_text_keys':len(keys),'translated_nonempty_entries':len(keys-EMPTY_TEXT),'glyphs':len(chars),'english_story_source':'original EXE'}

if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--folder',type=Path,default=ROOT/'languages');args=parser.parse_args();print(json.dumps(check(args.folder),indent=2))