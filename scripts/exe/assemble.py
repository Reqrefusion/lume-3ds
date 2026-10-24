"""Assemble an empty RomFS from producer outputs, never from an old game tree."""
from pathlib import Path
import struct,zlib,shutil,subprocess,json,io,os
import numpy as np
from PIL import Image
from video import read_exact
from state import sha,write_json

def frames(path):
 from lan_codec import records,index
 with Path(path).open('rb') as f:magic,w,h,fps,n=struct.unpack('<4s4I',f.read(20))
 if fps!=25:raise ValueError('LAN frame rate')
 for bg,x,y,cw,ch,raw in records(path):
  im=Image.new('RGBA',(w,h))
  if cw*ch:im.paste(Image.frombytes('RGBA',(cw,ch),raw),(x,y))
  yield bg,im


def resize(im,size):return im.resize(size,Image.Resampling.LANCZOS)

def lan(path,images,w=320,h=240):
 path=Path(path);path.parent.mkdir(parents=True,exist_ok=True);spool=path.with_suffix('.spool');offsets=[]
 with spool.open('wb') as f:
  for bg,im in images:
   if im.size!=(w,h):im=resize(im,(w,h))
   box=im.getbbox();offsets.append(f.tell())
   if not box:f.write(struct.pack('<IhhHH',bg,0,0,0,0));continue
   x,y,r,b=box;f.write(struct.pack('<IhhHH',bg,x,y,r-x,b-y)+zlib.compress(im.crop(box).tobytes(),6))
  end=f.tell()
 n=len(offsets)
 if not n:raise ValueError('Empty LAN')
 head=20+4*(n+1)
 temporary=path.with_name(path.name+'.creating.tmp')
 with temporary.open('wb') as f,spool.open('rb') as src:
  f.write(struct.pack('<4s4I',b'LAN2',w,h,25,n)+struct.pack('<%dI'%(n+1),*[head+x for x in offsets+[end]]));shutil.copyfileobj(src,f)
  f.flush();os.fsync(f.fileno())
 os.replace(temporary,path)
 spool.unlink()

def dzp(path,raw):
 path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b'DZP1'+struct.pack('<II',800,600)+zlib.compress(raw,6))

def page_alias(path,name):
 ref=('detail/'+name+'.dzp').encode('ascii');path.write_bytes(b'LRF1'+struct.pack('<H',len(ref))+ref)

def movie_frame(path,index):
 import lz4.block
 with Path(path).open('rb') as probe:magic=probe.read(4)
 if magic==b'LVM1':
  from mpeg_video import read_frame,resolve_ffmpeg
  left,right,_=read_frame(path,index,resolve_ffmpeg(path));return left,right
 with Path(path).open('rb') as f:
  magic,w,h,fps,n,key=struct.unpack('<4sHHIII',f.read(20));ix=struct.unpack('<%dI'%(n+1),f.read(4*(n+1)))
  if magic not in [b'LVS1',b'LVS2'] or (w,h,fps)!=(320,240,25) or not 0<=index<n:raise ValueError('Movie request out of bounds')
  prev=None
  for fr in range(index-index%4 if magic==b'LVS2' else index,index+1):
   f.seek(ix[fr]);packet=f.read(ix[fr+1]-ix[fr]);mode=packet[0] if magic==b'LVS2' else 0;body=packet[1:] if magic==b'LVS2' else packet
   raw=lz4.block.decompress(body,uncompressed_size=230400)
   if mode:
    a=np.frombuffer(raw,np.uint8);p=np.frombuffer(prev,np.uint8);v=np.empty(230400,np.uint8);v[:153600:2]=a[:76800]+p[:153600:2];v[1:153600:2]=a[76800:153600]+p[1:153600:2];v[153600:]=a[153600:];raw=v.tobytes()
   prev=raw
 c=np.frombuffer(raw[:153600],'<u2').reshape(240,320);r,g,b=c>>11,(c>>5)&63,c&31;left=np.stack([(r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2)],axis=2).astype(np.uint8)
 d=np.frombuffer(raw[153600:],np.int8).reshape(240,320);yy,xx=np.mgrid[:240,:320];q=(xx*4+d).clip(0,1276);sx=q>>2;f=(q&3)[:,:,None]
 right=((left[yy,sx].astype(np.uint16)*(4-f)+left[yy,np.minimum(sx+1,319)].astype(np.uint16)*f+2)>>2).astype(np.uint8)
 return Image.fromarray(left),Image.fromarray(right)

def prop(path):
 d=path.read_bytes();magic,w,h,x,y,inv=struct.unpack_from('<4sIIfff',d)
 if magic!=b'PRP1' or len(d)!=24+w*h*4:raise ValueError('PRP bounds')
 return Image.frombytes('RGBA',(w,h),d[24:]),x,y,inv

def overlay_prop(image,path,m,eye=0):
 im,x,y,inv=prop(path);a,b,c,d,tx,ty=m
 mat=np.array([[.4*a*inv,.4*c*inv,.4*(a*x+c*y+tx)-eye],[.4*b*inv,.4*d*inv,.4*(b*x+d*y+ty)+13],[0,0,1]],float)
 if abs(np.linalg.det(mat))<1e-10:return image
 rev=np.linalg.inv(mat);coeff=tuple(rev[:2].reshape(-1));layer=im.transform((320,240),Image.Transform.AFFINE,coeff,Image.Resampling.BILINEAR)
 out=image.convert('RGBA');out.alpha_composite(layer);return out.convert('RGB')

def title_overview(path):
 # Extend the authored edge before reducing the padded RGBA canvas. Sampling
 # transparent padding beside an opaque black cover would reduce its alpha
 # at the viewport boundary and reveal a thin strip of the forest beneath.
 for bg,im in frames(path):
  if im.size!=(800,600):raise ValueError('Title source dimensions')
  guarded=im.copy()
  guarded.paste(im.crop((0,33,800,34)).resize((800,33),Image.Resampling.NEAREST),(0,0))
  guarded.paste(im.crop((0,565,800,566)).resize((800,34),Image.Resampling.NEAREST),(0,566))
  overview=resize(guarded,(320,240))
  overview.paste((0,0,0,0),(0,0,320,13))
  overview.paste((0,0,0,0),(0,226,320,240))
  yield bg,overview

def normal_and_detail(pages,title,motion,ui,rom):
 rom.mkdir(parents=True,exist_ok=True);detail=rom/'detail';detail.mkdir();(rom/'stereo').mkdir()
 for group in [pages,title]:
  for p in group.glob('*.rgb'):
   raw=p.read_bytes()
   if len(raw)!=800*600*3:raise ValueError('Source page dimensions: '+p.name)
   im=Image.frombytes('RGB',(800,600),raw);(rom/p.name).write_bytes(resize(im,(320,240)).tobytes());dzp(detail/(p.stem+'.dzp'),raw)
   if p.stem.startswith('clue_'):page_alias(rom/('read_'+p.stem[5:]+'.rgb'),p.stem)
  for p in group.glob('*.lan'):
   if p.name.startswith('ui_piano_'):continue
   if p.name=='studio_intro.lan':
    shutil.copyfile(p,rom/p.name);continue
   # Meter result previews use the native stream; no detail-camera reader uses a duplicate.
   if not p.name.startswith('ui_meter_'):shutil.copyfile(p,detail/p.name)
   lan(rom/p.name,title_overview(p) if p.stem in ['title_entry','title_idle','title_exit'] else frames(p))
 for p in title.glob('*.rgba'):
  raw=p.read_bytes();im=Image.frombytes('RGBA',(800,600),raw);lan(detail/(p.stem+'.lan'),[(4<<16,im)],800,600);(rom/p.name).write_bytes(resize(im,(320,240)).tobytes())
 for p in title.glob('title_touch*'):
  if p.suffix in ['.layout','.lta']:shutil.copyfile(p,detail/p.name)
 def piano():
  for n in ['ui_piano_key'+str(i) for i in range(1,9)]+['ui_piano_clear']:yield from frames(pages/(n+'.lan'))
 lan(detail/'ui_piano_keys.lan',piano(),800,600);lan(rom/'ui_piano_keys.lan',piano())
 for p in motion.iterdir():
  if p.is_file() and p.suffix in ['.lan','.sf2','.trf','.bin','.rgba','.rgb','.txt']:shutil.copyfile(p,rom/p.name)
 if (motion/'detail').exists():
  # Cinematic plates are played as film backgrounds. Their source-resolution
  # research copies are not a gameplay zoom asset and are not published.
  shutil.copytree(motion/'detail',detail,dirs_exist_ok=True,ignore=lambda folder,names:[n for n in names if n.startswith('cinema_bg') and n.endswith('.rgb')])
 shutil.copytree(ui,rom,dirs_exist_ok=True)
 # NAV metadata is derived from the exact completed packets, not guessed times.
 for p in rom.glob('nav_*.lan'):
  data=p.read_bytes();n=struct.unpack_from('<I',data,16)[0];ix=struct.unpack_from('<%dI'%(n+1),data,20);bgs=[struct.unpack_from('<I',data,a)[0] for a in ix[:-1]];mids={v>>16 for v in bgs}
  if len(mids)!=1 or next(iter(mids))>=4:raise ValueError('Navigation camera changes unexpectedly')
  p.with_suffix('.nav').write_bytes(b'NAV2'+struct.pack('<4I',bgs[0]>>16,min(v&65535 for v in bgs),max(v&65535 for v in bgs),max(b-a for a,b in zip(ix,ix[1:]))))
 # Idle captures reuse source landing frames, keeping feet/camera registration.
 feet=[(120,216),(204,158),(211,167),(148,172),(92,170),(113,185)]
 for i,name in enumerate(['nav_1_0','nav_0_1','nav_1_2','nav_2_3','nav_3_4','nav_3_5']):
  bg,im=list(frames(rom/(name+'.lan')))[-1];box=im.getbbox()
  if not box:raise ValueError('Empty landing actor: '+name)
  x,y,r,b=box;w,h=r-x,b-y
  if w>96 or h>96:raise ValueError('Unexpected landing crop: '+name+' '+str(box))
  (rom/f'actor_idle_{i}.bin').write_bytes(struct.pack('<4sHHhhhh',b'IDL1',w,h,x,y,*feet[i])+im.crop(box).tobytes())
  shutil.copyfile(detail/('actor_'+name+'.lan'),detail/(f'actor_idle_{i}.lan'))
 walk=[im for _,im in frames(rom/'walk_roof_ab.lan')][:18]+[list(frames(rom/'walk_roof_ba.lan'))[-1][1]]
 atlas=[]
 for im in walk:
  box=im.getbbox();im=im.crop(box);z=78/im.height;im=resize(im,(round(im.width*z),78));tile=Image.new('RGBA',(96,96));tile.alpha_composite(im,((96-im.width)//2,92-im.height));atlas.append(tile.tobytes())
 (rom/'avatar.rgba').write_bytes(b''.join(atlas))
 lan(rom/'anim_village.lan',[(1<<16|i,Image.new('RGBA',(320,240))) for i in range(150)])

def write_world_props(original,rom):
 states=original.states(988)
 matrices=[value for frame in [1,131] for depth in [135,148,155] for value in states[frame][depth]['matrix']]
 (Path(rom)/'world_props.trf').write_bytes(struct.pack('<4sII36f',b'TRF1',2,3,*matrices))

def world(swf,recovered,videos,motion,rom,ffmpeg,java=None,jar=None):
 from original_swf import Original
 from close_controls import produce as close_controls
 root=Path(__file__).resolve().parents[2]
 java=java or next((root/'tools/detail/java').rglob('java.exe'));jar=jar or root/'tools/detail/ffdec/ffdec.jar'
 close_controls(root,swf,rom,java,jar)
 o=Original(swf);states=o.states(988);write_world_props(o,rom);ids={175:0,740:1,940:2,945:3};names=['0175','0740','0940','0945'];needed=set()
 def bg(fr):
  s=states[fr];e=s.get(1,{})
  if e.get('character') not in ids:e=s[116]
  return ids[e['character']],e.get('ratio',0)
 for name,fr in [('garden',1),('roof',131),('front',399),('hall',699),('upstairs',969),('power',1603)]:
  mid,idx=0,{'garden':1602,'roof':974,'front':260,'hall':377,'upstairs':501,'power':797}[name];needed.add((mid,idx));pair=movie_frame(videos/names[mid]/('media_'+names[mid]+'.lvs'),idx)
  if name in ['garden','roof']:
   (rom/('world_clean_'+name+'.rgb')).write_bytes(pair[0].tobytes())
  for stage in range(1,6) if name in ['garden','roof'] else [1]:
   for bare in ([False,True] if name in ['garden','roof'] and stage<4 else [False]):
    suffix=('' if stage==1 else str(stage))+('_bare' if bare else '')
    for eye in range(2):
     im=pair[eye]
     if name in ['garden','roof']:
      for depth,asset in [(135,'turbine'+str(stage)),(148,'fence0' if bare or stage>=4 else 'fence1'),(155,'solar')]:
       im=overlay_prop(im,motion/('prop_'+asset+'.bin'),states[fr][depth]['matrix'],eye)
     raw=im.tobytes();(rom/'stereo'/(name+suffix+('_R.rgb' if eye else '_L.rgb'))).write_bytes(raw)
     if not eye:(rom/('world_'+name+suffix+'.rgb')).write_bytes(raw)
 for p in rom.glob('nav_*.lan'):
  bgkey,_=list(frames(p))[-1]
  if bgkey>>16<4:needed.add((bgkey>>16,bgkey&65535))
 for mid,idx in sorted(needed):
  src=recovered/'video'/(names[mid]+'.flv');raw=subprocess.check_output([str(ffmpeg),'-v','error','-nostdin','-i',str(src),'-vf',f'select=eq(n\\,{idx}),pad=800:600:0:33:black','-frames:v','1','-fps_mode','passthrough','-f','rawvideo','-pix_fmt','rgb24','pipe:1'])
  if len(raw)!=1440000:raise ValueError('High-resolution world frame missing: '+str((mid,idx)))
  dzp(rom/'detail'/('world_'+names[mid]+'_%04d.dzp'%idx),raw)
 # Last credits composition is the original endpoint, with the same background.
 key,overlay=list(frames(rom/'anim_ending.lan'))[-1];mid,idx=key>>16,key&65535
 pair=[Image.frombytes('RGB',(320,240),(rom/f'cinema_bg{idx}.rgb').read_bytes())]*2 if mid==4 else movie_frame(videos/names[mid]/('media_'+names[mid]+'.lvs'),idx)
 for eye in range(2):
  im=pair[eye].convert('RGBA');im.alpha_composite(overlay);(rom/('ending_'+('R' if eye else 'L')+'.rgb')).write_bytes(im.convert('RGB').tobytes())

def fallback_atlases(motion,pages,recovered,rom):
 source=Image.open(motion/'solar_source.png').convert('RGBA');pattern=source.crop((1,2,401,402));base=Image.new('RGB',(400,400),'#10191c');base.paste(pattern,(0,0),pattern);(rom/'solar_pattern.rgb').write_bytes(resize(base,(160,160)).tobytes())
 tiles=bytearray(b'SLR1')
 for side in [19,38]:
  for cell in range(64):
   row,col=divmod(cell,8);tile=source.crop((1+col*50,2+row*50,51+col*50,52+row*50))
   for rot in range(4):tiles.extend(resize(tile.rotate(-rot*90),(side,side)).tobytes())
 (rom/'solar_tiles.bin').write_bytes(tiles)
 stamps=list(frames(pages/'map_ink.lan'))
 if len(stamps)!=4:raise ValueError('Four original map ink variants required')
 data=bytearray(b'INK2')
 for _,ink in stamps:
  box=ink.getbbox()
  if not box:raise ValueError('Source map highlight missing')
  stamp=ink.crop(box)
  for side,scale in [(32,.4),(64,.8)]:
   # The slot is padding, not a target glyph size. Preserve the original
   # 800-pixel-stage dimensions at overview .4x and legacy detail .8x.
   size=(max(1,round(stamp.width*scale)),max(1,round(stamp.height*scale)))
   if max(size)>side:raise ValueError('Source map ink exceeds its atlas slot')
   part=resize(stamp,size);out=Image.new('RGBA',(side,side));out.alpha_composite(part,((side-part.width)//2,(side-part.height)//2));data.extend(out.tobytes())
 (rom/'map_ink.bin').write_bytes(data)

def digits(swf,java,jar,work,rom):
 from original_swf import Original
 from swf_encoding import tag,place
 from recover import Bits
 o=Original(swf);b=Path(swf).read_bytes()[8:];bits=Bits(b);bits.rect();p=bits.p//8;prefix=b[:p+2]+struct.pack('<H',60)
 def compose(a,b):
  aa,ab,ac,ad,ax,ay=a;ba,bb,bc,bd,bx,by=b
  return [aa*ba+ac*bb,ab*ba+ad*bb,aa*bc+ac*bd,ab*bc+ad*bd,aa*bx+ac*by+ax,ab*bx+ad*by+ay]
 layouts=[]
 for row in [[.496154785,-.86555481,.86555481,.496154785,251.05,199.4],[.996994019,0,0,.996994019,383,98.65],[.6022644,.7963104,-.7963104,.6022644,527.95,150.7]]:
  for cid in [692,694,695,696,697,698,699,700,701,702]:layouts.append((cid,compose(row,[1,0,0,1,-60.55,14.25])))
 for x,y in [(326.75,248.4),(390.6,247.9),(448.35,248.4)]:
  for cid in [525,531,533,535,537,539,541,543,545,526]:layouts.append((cid,[1,0,0,1,x-59.5,y+24.8]))
 stream=prefix+o.definitions()+tag(9,b'\xff\xff\xff')
 for i,(cid,tr) in enumerate(layouts):
  if i:stream+=tag(28,struct.pack('<H',1))
  stream+=place(cid,1,tr)+tag(1,b'')
 stream+=tag(0,b'');work.mkdir(parents=True,exist_ok=True);path=work/'digits.swf';path.write_bytes(b'FWS\x09'+struct.pack('<I',len(stream)+8)+stream)
 subprocess.run([str(java),'-Xmx768m','-Djava.awt.headless=true','-jar',str(jar),'-onerror','abort','-export','frame',str(work/'frames'),str(path)],check=True,stdout=subprocess.DEVNULL)
 images=sorted((work/'frames').rglob('*.png'),key=lambda p:int(p.stem))
 if len(images)!=60:raise ValueError('Original digit frames missing')
 positions=[(117,92),(162,67),(205,89),(136,129),(162,129),(185,129)]
 for name,start in [('door',0),('cupboard',30)]:
  atlas=bytearray()
  for i in range(30):
   im=Image.open(images[start+i]).convert('RGB').resize((320,213),Image.Resampling.LANCZOS);canvas=Image.new('RGB',(320,240),'white');canvas.paste(im,(0,13));cx,cy=positions[start//10+i//10];glyph=canvas.crop((cx-16,cy-16,cx+16,cy+16)).convert('L');atlas.extend(bytes(255-x for x in glyph.tobytes()))
  (rom/('digits_'+name+'.alpha')).write_bytes(atlas)

def still_stereo(rom,model,preview_root=None):
 from stereo_depth import Stereo
 stereo=Stereo(model);dest=rom/'stereo';dest.mkdir(exist_ok=True)
 from production_previews import settings
 preview=settings(preview_root) if preview_root else None
 model_hash=sha(model)
 paths=list(rom.glob('clue_*.rgb'))+list(rom.glob('world_pantry_*.rgb'))+[rom/(n+'.rgb') for n in ['wires','piano','solar','keys','cupboard','map','door','solar_pattern']]+list(rom.glob('cinema_bg[0-9]*.rgb'))
 for p in paths:
  if p.stem.endswith('_R'):continue
  side=(160,160) if p.stem=='solar_pattern' else (320,240)
  raw=p.read_bytes()
  if len(raw)!=side[0]*side[1]*3:raise ValueError('Normal still dimensions: '+p.name)
  stereo.strength=3.5 if p.stem.startswith(('clue_','cinema_bg')) else 5
  rgb=np.frombuffer(raw,np.uint8).reshape(side[1],side[0],3);left,right,depth=stereo.pair(rgb)
  if preview and preview['capture_neural_depth']:
   from stereo_preview import save_pair
   folder=Path(preview_root)/'previews/production/stills'/p.stem
   save_pair(folder,left,right);np.save(folder/'depth.npy',depth)
   Image.fromarray(np.rint(depth.clip(0,1)*65535).astype(np.uint16)).save(folder/'depth16.png')
   Image.fromarray(np.rint(depth.clip(0,1)*255).astype(np.uint8)).save(folder/'depth_view.png')
   write_json(folder/'method.json',{'image':p.name,'model_sha256':model_hash,'strength':stereo.strength,'stage':'actual still-image neural production before game packing'})
  if p.stem in ['wires','keys','door','cupboard']:
   # Transfer the same right-to-left displacement used by the stereo video
   # path. The runtime applies it after compositing the current board state.
   import cv2
   flow=cv2.calcOpticalFlowFarneback(cv2.cvtColor(right,cv2.COLOR_RGB2GRAY),cv2.cvtColor(left,cv2.COLOR_RGB2GRAY),None,.5,3,19,4,5,1.1,0)[:,:,0]
   flow=np.clip(flow,-3,5);flow[:13]=0;flow[226:]=0
   displacement=np.rint(flow*4).clip(-128,127).astype(np.int8)
   (dest/(p.stem+'.depth')).write_bytes(struct.pack('<4sHH',b'PDP1',320,240)+displacement.tobytes())
  if p.stem.startswith('cinema_bg'):(rom/(p.stem+'_R.rgb')).write_bytes(right.tobytes())
  else:
   name=p.stem.removeprefix('clue_').removeprefix('world_');(dest/(name+'_L.rgb')).write_bytes(left.tobytes());(dest/(name+'_R.rgb')).write_bytes(right.tobytes())
