"""Build only compact still correspondence fields from the existing eye pairs.

No video decoding, neural model inference or game raster replacement. RGB24
endpoints stay byte-exact. Farneback is the same right-to-left convention used
by the game's video and interactive puzzle displacement maps.
"""
from pathlib import Path
import sys,struct,os,json,tempfile,hashlib
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE));sys.path.insert(0,str(HERE.parent))

def raster(path):
 import numpy as np,lz4.block
 from compress import restore
 data=Path(path).read_bytes()
 if data[:4]==b'LRC1':
  n,w,h,c,mode,version,reserved=struct.unpack_from('<IHHBBBB',data,4)
  if c!=3 or version!=1 or reserved or w*h*c!=n or w>320 or h>240:raise ValueError('RGB header: '+str(path))
  data=restore(lz4.block.decompress(data[16:],uncompressed_size=n),w,c,mode)
 else:
  if len(data)==320*240*3:w,h=320,240
  elif len(data)==160*160*3:w,h=160,160
  else:raise ValueError('RGB size: '+str(path))
 return np.frombuffer(data,np.uint8).reshape(h,w,3).copy()

def pairs(rom):
 found={}
 for p in sorted((rom/'stereo').glob('*_L.rgb')):
  r=p.with_name(p.name[:-6]+'_R.rgb')
  if r.is_file():found[p]=r
 for pattern,prefix in [('world_*.rgb','world_'),('clue_*.rgb','clue_')]:
  for p in sorted(rom.glob(pattern)):
   r=rom/'stereo'/(p.stem.removeprefix(prefix)+'_R.rgb')
   if r.is_file():found[p]=r
 for p in sorted(rom.glob('cinema_bg[0-9]*.rgb')):
  if p.stem.endswith('_R'):continue
  r=p.with_name(p.stem+'_R.rgb')
  if r.is_file():found[p]=r
 if (rom/'ending_L.rgb').is_file() and (rom/'ending_R.rgb').is_file():found[rom/'ending_L.rgb']=rom/'ending_R.rgb'
 return sorted(found.items())

def encode(left,right):
 import cv2,numpy as np,lz4.block
 if left.shape!=right.shape:raise ValueError('Stereo pair sizes differ')
 h,w=left.shape[:2]
 flow=cv2.calcOpticalFlowFarneback(cv2.cvtColor(right,cv2.COLOR_RGB2GRAY),cv2.cvtColor(left,cv2.COLOR_RGB2GRAY),None,.5,3,19,4,5,1.1,0)[:,:,0]
 # Full stored right eye remains the endpoint. Only intermediate geometry is
 # estimated from it; never blend two shifted photographs and create ghosts.
 disp=np.rint(np.nan_to_num(flow,nan=0,posinf=0,neginf=0)*4).clip(-128,127).astype(np.int8)
 raw=disp.tobytes();packed=lz4.block.compress(raw,mode='high_compression',compression=9,store_size=False)
 if lz4.block.decompress(packed,uncompressed_size=w*h)!=raw:raise ValueError('Field compression mismatch')
 return struct.pack('<4sHH',b'SDF1',w,h)+packed

def build(rom):
 rom=Path(rom);records=[];written={}
 try:
  for left,right in pairs(rom):
   a=raster(left);b=raster(right);data=encode(a,b);path=Path(str(left)+'.sdf')
   previous=path.read_bytes() if path.is_file() else None
   if previous!=data:
    written[path]=previous;tmp=path.with_name(path.name+'.tmp');tmp.write_bytes(data);os.replace(tmp,path)
   records.append({'left':left.relative_to(rom).as_posix(),'right':right.relative_to(rom).as_posix(),'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
   print('Stereo kaydirac haritasi:',left.name,len(data),'bayt',flush=True)
  if not records:raise ValueError('No stereo RGB pair found in the current RomFS.')
 except Exception:
  rollback(written);raise
 return {'pairs':len(records),'total_bytes':sum(r['bytes'] for r in records),'video_conversion':False,'records':records},written

def rollback(written):
 for p,old in written.items():
  if old is None:p.unlink(missing_ok=True)
  else:p.write_bytes(old)

def run(rom):
 report,_=build(rom);return report

def main():
 from state import Lock,write_json
 root=HERE.parents[1];rom=root/'romfs'
 if not rom.is_dir():raise ValueError('No current game assets; complete EXE production first.')
 sys.path.insert(0,str(root/'build'))
 from manifest import save
 with Lock(root/'tools/exe-production/production.lock'):
  meta={p:p.read_bytes() if p.is_file() else None for p in [root/'assets.sha256.json',root/'source-manifest.json']}
  report,written=build(rom)
  try:save(root)
  except Exception:
   rollback(written)
   for p,old in meta.items():
    if old is None:p.unlink(missing_ok=True)
    else:p.write_bytes(old)
   raise
  write_json(root/'logs/stereo-fields.json',report)
 print('Still image maps ready. No video generated. Run BUILD.cmd.',flush=True)

if __name__=='__main__':
 try:main()
 except Exception as exc:print('ERROR:',exc);raise SystemExit(1)
