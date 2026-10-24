"""Vectorized reversible predictors; preserve every RGBA/depth sample exactly."""
from pathlib import Path
import struct,zlib,os
import numpy as np
import lz4.block

def predict(raw,w,c,mode):
 a=np.frombuffer(raw,np.uint8).reshape(-1,w,c)
 if mode==0:return raw
 out=a.copy()
 if mode==1:out[:,1:]=a[:,1:]-a[:,:-1]
 elif mode==2:out[1:]=a[1:]-a[:-1]
 else:raise ValueError('Predictor')
 return out.tobytes()

def restore(raw,w,c,mode):
 a=np.frombuffer(raw,np.uint8).reshape(-1,w,c)
 if mode==0:return raw
 if mode not in [1,2]:raise ValueError('Predictor')
 return np.cumsum(a,axis=1 if mode==1 else 0,dtype=np.uint8).tobytes()

def unpack_lan(prefix,body,magic):
 bg,x,y,w,h=struct.unpack('<IhhHH',prefix);mode=(bg>>28)&7 if magic==b'LAN4' else 0;fast=bool(bg&0x80000000) if magic in [b'LAN3',b'LAN4'] else False
 clean=bg&(0x0fffffff if magic==b'LAN4' else 0x7fffffff if magic==b'LAN3' else 0xffffffff)
 if w*h:
  if fast:raw=lz4.block.decompress(body,uncompressed_size=w*h*4)
  else:
   dec=zlib.decompressobj();raw=dec.decompress(body,w*h*4+1)
   if len(raw)!=w*h*4 or not dec.eof or dec.unused_data or dec.unconsumed_tail:raise ValueError('LAN zlib')
  raw=restore(raw,w,4,mode)
 else:
  if body:raise ValueError('Empty LAN packet')
  raw=b''
 return clean,x,y,w,h,raw

def compress_lan(path):
 from lan_codec import pack
 return pack(path)


def static(path):
 path=Path(path);data=path.read_bytes();c=3 if path.suffix=='.rgb' else 4
 if len(data)==320*240*c:w,h=320,240
 elif len(data)==160*160*c:w,h=160,160
 elif len(data)==18*18*c:w,h=18,18
 else:return
 best=None
 for mode in range(3):
  body=lz4.block.compress(predict(data,w,c,mode),mode='high_compression',compression=9,store_size=False);candidate=b'LRC1'+struct.pack('<IHHBBBB',len(data),w,h,c,mode,1,0)+body
  if len(candidate)<len(data) and (best is None or len(candidate)<len(best)):
   if restore(lz4.block.decompress(body,uncompressed_size=len(data)),w,c,mode)!=data:raise ValueError('Raster lossless proof')
   best=candidate
 if best:path.write_bytes(best)

def run(rom):
 rom=Path(rom);before=sum(p.stat().st_size for p in rom.rglob('*') if p.is_file());files=[p for p in rom.rglob('*') if p.is_file()]
 from state import output_files
 files=list(output_files(rom));lan_reports=[]
 for i,p in enumerate(files):
  if p.suffix in ['.lan','.lta']:
   report=compress_lan(p);report['file']=p.relative_to(rom).as_posix();lan_reports.append(report)
  elif p.suffix in ['.rgb','.rgba']:static(p)
  if i%100==0:print('Lossless packaging',i,'/',len(files),flush=True)
 for p in rom.glob('nav_*.lan'):
  d=p.read_bytes();magic,w,h,fps,n=struct.unpack_from('<4s4I',d);ix=struct.unpack_from('<%dI'%(n+1),d,20);bg=[struct.unpack_from('<I',d,a)[0]&(0x03ffffff if magic==b'LAN5' else 0x0fffffff) for a in ix[:-1]]
  p.with_suffix('.nav').write_bytes(b'NAV2'+struct.pack('<4I',bg[0]>>16,min(v&65535 for v in bg),max(v&65535 for v in bg),max(b-a for a,b in zip(ix,ix[1:]))))
 after=sum(p.stat().st_size for p in rom.rglob('*') if p.is_file())
 groups={}
 for p in output_files(rom):
  category='videos' if p.suffix=='.lvs' else 'animations' if p.suffix in ['.lan','.lta'] else 'other'
  groups[category]=groups.get(category,0)+p.stat().st_size
 print('Game assets:',round(after/1048576,2),'MiB; video:',round(groups.get('videos',0)/1048576,2),'MiB; animation:',round(groups.get('animations',0)/1048576,2),'MiB',flush=True)
 return {'before_bytes':before,'after_bytes':after,'scope':'Current produced RomFS only; SDK, intermediate media, backups and previews excluded. Before is this run before LAN/static packing, not the previous game build.','asset_groups_bytes':groups,'pixel_alpha_and_disparity_preserved':True,'lan':lan_reports}
