"""Runtime asset contract, including dynamic paths and authored clip sidecars."""
from pathlib import Path
import struct,json,re,zlib
from recipe import NAV,WALKS
from assemble import frames
from state import sha,output_files

def required(rom):
 r=set();add=lambda *names:r.update(names)
 puzzles=['wires','piano','solar','keys','cupboard','map','door']
 for n in puzzles+['solar_pattern']:add(n+'.rgb',*(f'stereo/{n}_{e}.rgb' for e in ['L','R']))
 add('ui_meter_zero_half.lan','ui_meter_half_full.lan','world_props.trf','ui_close.bin')
 add('lang/languages.txt','solar_completed.rgb','solar_tiles.bin','map_ink.bin','digits_door.alpha','digits_cupboard.alpha','ui_close.rgba','avatar.rgba','speech_labels.txt','hover.bin')
 add(*(f'actor_idle_{i}.bin' for i in range(6)),*(f'detail/actor_idle_{i}.lan' for i in range(6)))
 for n in ['garden','roof','front','hall','upstairs','power']+['garden'+str(i) for i in range(2,6)]+['roof'+str(i) for i in range(2,6)]:add('world_'+n+'.rgb',*(f'stereo/{n}_{e}.rgb' for e in ['L','R']))
 clues=['letter','book','book2']+['book3_'+str(i) for i in range(11)]+['photo'+str(i) for i in range(1,4)]+['meter'+str(i) for i in range(3)]+['cupboard_m'+str(i) for i in range(4)]
 for n in clues:add('clue_'+n+'.rgb','detail/clue_'+n+'.dzp',*(f'stereo/{n}_{e}.rgb' for e in ['L','R']))
 for n in ['letter','book','book2']+['book3_'+str(i) for i in range(11)]+['photo'+str(i) for i in range(1,4)]:add('read_'+n+'.rgb')
 for b in range(1,4):
  for m in range(8):
   n=f'pantry_b{b}_m{m}';add('world_'+n+'.rgb','detail/world_'+n+'.dzp',*(f'stereo/{n}_{e}.rgb' for e in ['L','R']))
 for n in NAV:add(n+'.lan',n+'.trf',n+'.sf2',n+'.nav','detail/actor_'+n+'.lan')
 for n in WALKS:add(n+'.lan',n+'.sf2','detail/actor_'+n+'.lan')
 add('walk_front_try.lan','walk_front_try.sf2','detail/actor_walk_front_try.lan','anim_intro.lan','anim_intro.sf2','anim_ending.lan','anim_ending.sf2','anim_village.lan','fx_gramophone.lan','ending_L.rgb','ending_R.rgb')
 for n in ['turbine'+str(i) for i in range(1,6)]+['fence0','fence1','solar']:add('prop_'+n+'.bin')
 for n in ['entry','idle','exit']:add('title_'+n+'.lan','detail/title_'+n+'.lan','detail/title_touch_'+n+'.lta')
 for fr in [1602,974,260,377,501,797]:add('detail/world_0175_%04d.dzp'%fr)
 add('title_motion_base.rgb','detail/title_motion_base.dzp','detail/title_touch.layout','detail/reading/font.bin','detail/reading/paper320.bin','detail/reading/paper400.bin','detail/native_menu/art.bin','detail/native_menu/font.bin')
 for n in ['continue','credits','confirm']:add('detail/title_'+n+'.lan')
 for n in ['art_keys','art_wires','ui_piano_keys','ui_cupboard_progress','ui_result_timer']:add(n+'.lan','detail/'+n+'.lan')
 for n in ['digits_door','digits_cupboard']:add(n+'.lan')
 for n in ['digits_door','digits_cupboard','solar_tiles','map_ink']:add('detail/'+n+'.lan')
 add('map_ink.lan','ending_reading.tsv','detail/reading/page_ending.txt',*(f'detail/reading/ending_{i}.txt' for i in [964,967,968,969,970,971,972]))
 for scene in ['garden','roof']:
  add('world_clean_'+scene+'.rgb')
  for stage in ['', '2', '3']:
   n=scene+stage+'_bare';add('world_'+n+'.rgb',*(f'stereo/{n}_{e}.rgb' for e in ['L','R']))
 for name in ['wires','keys','door','cupboard']:add('stereo/'+name+'.depth')
 add('title_entry.tsf','title_idle.tsf','title_stereo.json','studio_intro.lan')
 for cid in ['0175','0740','0940','0945']:add('media_'+cid+'.lvs')
 from stereo_fields import pairs
 for left,_ in pairs(rom):add(left.relative_to(rom).as_posix()+'.sdf')
 return r

def validate(root,rom):
 root,rom=Path(root),Path(rom);expected=required(rom);missing=sorted(n for n in expected if not (rom/n).is_file());errors=[]
 if missing:return {'complete_game_assets':False,'missing':missing,'errors':errors}
 for name in ['wires','keys','door','cupboard']:
  d=(rom/'stereo'/(name+'.depth')).read_bytes()
  if len(d)!=76808 or d[:8]!=struct.pack('<4sHH',b'PDP1',320,240):errors.append(name+': puzzle disparity header/payload')
 ink=(rom/'map_ink.bin').read_bytes()
 if len(ink)!=4+4*(32*32+64*64)*4 or ink[:4]!=b'INK2':errors.append('map_ink.bin: four natural-scale variants required')
 for name in ['map_ink.lan','detail/map_ink.lan']:
  if not (rom/name).is_file() or struct.unpack_from('<I',(rom/name).read_bytes(),16)[0]!=4:errors.append(name+': four original ink frames required')
 # Validate every published raster payload, including compressed source pages.
 from compress import restore
 import lz4.block
 for p in output_files(rom):
  rel=p.relative_to(rom)
  if p.suffix=='.sdf':
   d=p.read_bytes()
   if len(d)<9 or d[:4]!=b'SDF1':errors.append(str(rel)+': stereo field header');continue
   w,h=struct.unpack_from('<HH',d,4)
   if (w,h) not in [(320,240),(160,160)]:errors.append(str(rel)+': stereo field size');continue
   try:raw=lz4.block.decompress(d[8:],uncompressed_size=w*h)
   except (ValueError,lz4.block.LZ4BlockError):errors.append(str(rel)+': stereo field payload');continue
   if len(raw)!=w*h:errors.append(str(rel)+': stereo field length')
  elif p.suffix=='.dzp':
   d=p.read_bytes()
   if d[:4] not in [b'DZP1',b'DZP2',b'DZP3'] or struct.unpack_from('<II',d,4)!=(800,600):errors.append(str(rel)+': page header');continue
   from fast_assets import dzp_decode
   try:dzp_decode(d)
   except (ValueError,zlib.error,lz4.block.LZ4BlockError):errors.append(str(rel)+': page payload')
  elif p.suffix in ['.rgb','.rgba']:
   d=p.read_bytes()
   if p.name.startswith('read_'):
    if len(d)<6 or d[:4]!=b'LRF1':errors.append(str(rel)+': page alias');continue
    n=struct.unpack_from('<H',d,4)[0];name=d[6:].decode('ascii')
    if n!=len(d)-6 or not name.startswith('detail/') or '..' in name or not (rom/name).is_file():errors.append(str(rel)+': alias target')
    continue
   c=3 if p.suffix=='.rgb' else 4;size=160*160*c if p.stem.startswith('solar_pattern') else 18*18*4 if p.name=='ui_close.rgba' else 19*96*96*4 if p.name=='avatar.rgba' else 320*240*c
   if len(d)==size:continue
   if len(d)<17 or d[:4]!=b'LRC1':errors.append(str(rel)+': raster format');continue
   n,w,h,channels,mode,kind,reserved=struct.unpack_from('<IHHBBBB',d,4)
   if n!=size or w*h*channels!=n or channels!=c or mode>2 or kind!=1 or reserved:errors.append(str(rel)+': raster header');continue
   raw=lz4.block.decompress(d[16:],uncompressed_size=n)
   if len(restore(raw,w,channels,mode))!=n:errors.append(str(rel)+': raster payload')
 for n,(a,b) in NAV.items():
  p=rom/(n+'.lan');h=p.read_bytes()[:20];actual=struct.unpack_from('<I',h,16)[0]
  if actual!=b-a+1:errors.append(n+': source frame count differs')
  trf=(rom/(n+'.trf')).read_bytes()
  if trf[:4]!=b'TRF1' or struct.unpack_from('<II',trf,4)!=(actual,3) or len(trf)!=12+actual*72:errors.append(n+': TRF registration')
 for n,(cam,cid,fs,mi) in WALKS.items():
  if struct.unpack_from('<I',(rom/(n+'.lan')).read_bytes(),16)[0]!=len(fs):errors.append(n+': authored child count differs')
 # Inspect all frames and their background references before publishing.
 counts={}
 try:
  title_meta=json.loads((rom/'title_stereo.json').read_text(encoding='utf-8'))
  from logo_depth import POLICY,quarter_pixels
  if title_meta.get('logo_policy_sha256')!=sha(POLICY) or title_meta.get('logo_quarter_pixels')!=quarter_pixels():raise ValueError('logo depth policy differs')
  if title_meta['version']!=1 or title_meta['base_sha256']!=sha(rom/'title_touch_top.rgb'):raise ValueError('title base changed')
  if {row['phase'] for row in title_meta['phases']}!={'entry','idle'}:raise ValueError('title phases differ')
  for row in title_meta['phases']:
   phase=row['phase']
   if row['source_sha256']!=sha(rom/('title_'+phase+'.lan')) or row['corner_sha256']!=sha(rom/'detail'/('title_touch_'+phase+'.lta')) or row['depth_sha256']!=sha(rom/('title_'+phase+'.tsf')):raise ValueError('title colour/depth identities differ')
 except (OSError,ValueError,KeyError,TypeError) as error:errors.append('title_stereo.json: '+str(error))
 for phase in ['entry','idle']:
  p=rom/('title_'+phase+'.tsf')
  if not p.is_file():continue
  try:
   data=p.read_bytes();magic,w,h,n=struct.unpack_from('<4sIII',data)
   expected_n=struct.unpack_from('<I',(rom/('title_'+phase+'.lan')).read_bytes(),16)[0]
   if magic!=b'TSF1' or (w,h)!=(160,120) or n!=expected_n:raise ValueError('title depth header')
   offsets=struct.unpack_from('<%dI'%(n+1),data,16)
   if offsets[0]!=16+4*(n+1) or offsets[-1]!=len(data):raise ValueError('title depth offsets')
   for a,b in zip(offsets,offsets[1:]):
    if not 0<b-a<=20000 or len(lz4.block.decompress(data[a:b],uncompressed_size=19200))!=19200:raise ValueError('title depth payload')
  except (ValueError,struct.error,RuntimeError) as error:errors.append(str(p.relative_to(rom))+': '+str(error))
 for p in rom.glob('media_*.lvs'):
  with p.open('rb') as video_file:header_bytes=video_file.read(20)
  magic,w,h,fps,n,key=struct.unpack('<4sHHIII',header_bytes)
  if magic==b'LVM1':
   from mpeg_video import validate_file
   try:validate_file(p)
   except (ValueError,struct.error) as e:errors.append(p.name+': '+str(e))
  elif magic!=b'LVS2' or (w,h,fps,key)!=(320,240,25,4):errors.append(p.name+': video format')
  counts[p.stem[6:]]=n
 try:
  data=(rom/'world_props.trf').read_bytes()
  if len(data)!=156 or struct.unpack_from('<4sII',data)!=(b'TRF1',2,3) or not all(__import__('math').isfinite(v) and abs(v)<=10000 for v in struct.unpack_from('<36f',data,12)):raise ValueError('world_props.trf: geometry')
 except (OSError,ValueError,struct.error) as e:errors.append(str(e))
 try:
  data=(rom/'ui_close.bin').read_bytes()
  magic,x,y,w,h=struct.unpack_from('<4sHHHH',data)
  if magic!=b'XUI1' or not(0<w<=32 and 0<h<=32 and x+w<=29 and y>=13 and y+h<=44) or len(data)!=12+w*h*4:raise ValueError('ui_close.bin: UI layout')
 except (OSError,ValueError,struct.error) as e:errors.append(str(e))
 from sound_events import validate_bank,BASE_ID
 try:event_ids=validate_bank(rom)
 except ValueError as exc:errors.append(str(exc));event_ids=set()
 movie_ids=['0175','0740','0940','0945']
 for p in rom.glob('*.lan'):
  h=p.read_bytes()[:20];n=struct.unpack_from('<I',h,16)[0];side=p.with_suffix('.sf2')
  if side.is_file():
   d=side.read_bytes()
   if len(d)<12+4*(n+1) or d[:4]!=b'SFX2' or struct.unpack_from('<I',d,4)[0]!=n:errors.append(side.name+': sound frame count')
   else:
    count=struct.unpack_from('<I',d,8)[0];ix=struct.unpack_from('<%dI'%(n+1),d,12)
    if ix[0]!=0 or ix[-1]!=count or any(b<a for a,b in zip(ix,ix[1:])) or len(d)!=12+4*(n+1)+2*count:errors.append(side.name+': sound index bounds')
    if len(d)==12+4*(n+1)+2*count:
     for ident in struct.unpack_from('<%dH'%count,d,12+4*(n+1)):
      if ident>=BASE_ID and ident not in event_ids:errors.append(side.name+': missing authored event '+str(ident))
  for bg,im in frames(p):
   mid,fr=bg>>16,bg&65535
   if mid<4 and fr>=counts[movie_ids[mid]]:errors.append(p.name+': invalid camera reference '+str((mid,fr)));break
   if mid==4 and p.name.startswith('anim_') and not (rom/f'cinema_bg{fr}.rgb').is_file():errors.append(p.name+': missing cinema plate');break
 # Audio paths derive from the actual C source, rather than a duplicate list.
 from pipeline import effects
 for cid in effects(root/'source'):
  p=rom/f'sfx_{cid:04d}.pcm'
  if not p.is_file() or p.stat().st_size%2 or not p.stat().st_size:errors.append(p.name+': sound missing/invalid')
 for i in range(1,9):
  if not (rom/f'piano{i}.pcm').is_file():errors.append('piano'+str(i)+': missing')
 for n in ['0006','0946']:
  if not (rom/f'media_{n}.mp3').is_file():errors.append('music '+n+': missing')
 for line in (rom/'speech_labels.txt').read_text(encoding="utf-8").splitlines():
  for n in ['speech_'+line+'.rgba','detail/speech_'+line+'.lan']:
   if not (rom/n).is_file():errors.append(n+': speech missing')
 for pack in (root/'languages').glob('*.json'):
  lang=json.loads(pack.read_text(encoding="utf-8"));folder=rom/'lang'/lang['id']
  for n in ['font.bin','ui.txt']:
   if not (folder/n).is_file():errors.append(lang['id']+'/'+n+': missing')
  for k,v in lang['text'].items():
   if v.strip() and not (folder/(k+'.txt')).is_file():errors.append(lang['id']+'/'+k+': translation missing')
 for p in rom.glob('actor_idle_*.bin'):
  d=p.read_bytes();w,h=struct.unpack_from('<HH',d,4)
  if d[:4]!=b'IDL1' or not(1<=w<=96 and 1<=h<=96) or len(d)!=16+w*h*4:errors.append(p.name+': pose bounds')
 return {'complete_game_assets':not errors,'required_assets':len(expected),'total_files':sum(1 for p in output_files(rom)),'missing':missing,'errors':errors,'movie_frames':counts,'hardware_tested':False}
