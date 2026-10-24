"""Exact crop pixels, bounded temporal prediction and direct duplicate references.

Frame metadata and alpha never change. An anchor is required every eight
frames so a cold seek cannot trigger a whole-animation replay.
"""
import hashlib,struct,zlib,os
from pathlib import Path
import numpy as np
import lz4.block

REFERENCE=1<<27;TEMPORAL=1<<26;MASK=0x03ffffff;GROUP=8


def index(data):
 from compress import unpack_lan
 magic,w,h,fps,n=struct.unpack_from('<4s4I',data)
 if magic not in [b'LAN2',b'LAN3',b'LAN4',b'LAN5'] or not 1<=n<=10000 or (w,h) not in [(320,240),(800,600)] or not 1<=fps<=60:raise ValueError('LAN header')
 ix=struct.unpack_from('<%dI'%(n+1),data,20)
 if ix[0]!=20+4*(n+1) or ix[-1]!=len(data) or any(b-a<12 or b-a>4*1024*1024 for a,b in zip(ix,ix[1:])):raise ValueError('LAN index')
 return magic,w,h,fps,n,ix


def record(data,info,frame):
 magic,w,h,fps,n,ix=info
 if not 0<=frame<n:raise ValueError('LAN frame')
 a,b=ix[frame:frame+2];tag,x,y,cw,ch=struct.unpack_from('<IhhHH',data,a)
 if x<0 or y<0 or x+cw>w or y+ch>h or bool(cw)!=bool(ch):raise ValueError('LAN crop')
 return tag,x,y,cw,ch,data[a+12:b]


def pixels(data,info,frame,previous=None,shape=None):
 from compress import unpack_lan,restore
 tag,x,y,w,h,body=record(data,info,frame);magic=info[0]
 if magic!=b'LAN5':return unpack_lan(struct.pack('<IhhHH',tag,x,y,w,h),body,magic)
 clean=tag&MASK;mode=(tag>>28)&7
 if mode>2:raise ValueError('LAN predictor')
 if tag&REFERENCE:
  if tag&(TEMPORAL|0xf0000000) or len(body)!=4:raise ValueError('LAN reference flags')
  target=struct.unpack('<I',body)[0]
  if target>=frame or target//GROUP!=frame//GROUP:raise ValueError('LAN reference outside seek group')
  t,_,_,rw,rh,_=record(data,info,target)
  if t&(REFERENCE|TEMPORAL) or (rw,rh)!=(w,h):raise ValueError('LAN reference must be a matching independent crop')
  raw=pixels(data,info,target)[-1]
 else:
  if w*h:
   if tag>>31:raw=lz4.block.decompress(body,uncompressed_size=w*h*4)
   else:
    dec=zlib.decompressobj();raw=dec.decompress(body,w*h*4+1)
    if len(raw)!=w*h*4 or not dec.eof or dec.unused_data or dec.unconsumed_tail:raise ValueError('LAN zlib bounds')
   raw=restore(raw,w,4,mode)
  else:
   if body or tag&TEMPORAL:raise ValueError('LAN empty crop')
   raw=b''
  if tag&TEMPORAL:
   if frame%GROUP==0:raise ValueError('LAN missing group anchor')
   if previous is None:
    # Recursion is confined to at most seven earlier frames in this group.
    prior=pixels(data,info,frame-1);previous=prior[-1];shape=prior[3:5]
   if shape!=(w,h) or len(previous)!=len(raw):raise ValueError('LAN temporal dimensions')
   raw=(np.frombuffer(raw,np.uint8)+np.frombuffer(previous,np.uint8)).tobytes()
 return clean,x,y,w,h,raw


def records(path):
 data=Path(path).read_bytes();info=index(data);previous=None;shape=None
 for frame in range(info[4]):
  r=pixels(data,info,frame,previous,shape);yield r;previous=r[-1];shape=r[3:5]


def pack(path):
 from compress import predict,restore
 path=Path(path);data=path.read_bytes();info=index(data);magic,w,h,fps,n,ix=info
 encoded=[];seen={};previous=None;shape=None;counts={'absolute':0,'temporal':0,'reference':0,'empty':0}
 for i in range(n):
  if i%GROUP==0:seen.clear()
  bg,x,y,cw,ch,raw=pixels(data,info,i,previous,shape);key=(cw,ch,hashlib.sha256(raw).digest());best=b'';flags=0;kind='empty'
  if raw:
   candidates=[]
   for temporal in [False,True] if i%GROUP and shape==(cw,ch) else [False]:
    values=(np.frombuffer(raw,np.uint8)-np.frombuffer(previous,np.uint8)).tobytes() if temporal else raw
    for mode in range(3):
     diff=predict(values,cw,4,mode)
     candidates.append((zlib.compress(diff,6),(mode<<28)|(TEMPORAL if temporal else 0)))
     candidates.append((lz4.block.compress(diff,mode='high_compression',compression=9,store_size=False),0x80000000|(mode<<28)|(TEMPORAL if temporal else 0)))
   best,flags=min(candidates,key=lambda c:len(c[0]));kind='temporal' if flags&TEMPORAL else 'absolute'
   # Equal hashes nominate a duplicate; exact reconstruction confirms it.
   if key in seen and len(best)>4:
    target=seen[key]
    if pixels(data,info,target)[-1]==raw:best=struct.pack('<I',target);flags=REFERENCE;kind='reference'
   if kind=='absolute':seen.setdefault(key,i)
   if kind!='reference':
    restored=lz4.block.decompress(best,uncompressed_size=len(raw)) if flags>>31 else zlib.decompress(best)
    restored=restore(restored,cw,4,(flags>>28)&7)
    if flags&TEMPORAL:restored=(np.frombuffer(restored,np.uint8)+np.frombuffer(previous,np.uint8)).tobytes()
    if restored!=raw:raise ValueError('LAN exact pixel reconstruction')
  counts[kind]+=1;encoded.append(struct.pack('<IhhHH',bg|flags,x,y,cw,ch)+best);previous=raw;shape=cw,ch
 offsets=[20+4*(n+1)]
 for packet in encoded:offsets.append(offsets[-1]+len(packet))
 packed=struct.pack('<4s4I',b'LAN5',w,h,fps,n)+struct.pack('<%dI'%(n+1),*offsets)+b''.join(encoded)
 # Never grow an animation merely to change its container. Legacy readers
 # stay available, and the application can mix old and new files safely.
 if len(packed)<len(data):
  temporary=path.with_suffix(path.suffix+'.compressing');temporary.write_bytes(packed);os.replace(temporary,path)
 return {'file':path.name,'frames':n,'before_bytes':len(data),'after_bytes':min(len(packed),len(data)),'selected':'LAN5' if len(packed)<len(data) else magic.decode('ascii'),'modes':counts,'pixels_and_alpha_exact':True,'seek_group':GROUP}
