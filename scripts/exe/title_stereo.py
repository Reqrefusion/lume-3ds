"""Offline frame-indexed title depth; preserve source colour and touch layout."""
from pathlib import Path
import struct,os,hashlib,json

def produce(rom,model,dest=None):
 import numpy as np,cv2,lz4.block
 from PIL import Image
 from assemble import frames
 from stereo_fields import raster
 from stereo_depth import Stereo
 from logo_depth import title_plane,POLICY,quarter_pixels
 rom=Path(rom);dest=Path(dest) if dest else rom;dest.mkdir(parents=True,exist_ok=True)
 stereo=Stereo(model,strength=3);base=Image.fromarray(raster(rom/'title_touch_top.rgb')).convert('RGBA');report=[]
 for phase in ['entry','idle']:
  clip=rom/('title_'+phase+'.lan');corners=frames(rom/'detail'/('title_touch_'+phase+'.lta'))
  packets=[];stereo.previous=None
  for i,(_,overlay) in enumerate(frames(clip)):
   image=base.copy();array=np.array(overlay);array[:13]=0;array[226:]=0;array[13:33,:20]=0
   image.alpha_composite(Image.fromarray(array));_,corner=next(corners);image.alpha_composite(corner)
   rgb=np.array(image.convert('RGB'));depth=stereo.depth(rgb[13:226],temporal=True)
   full=np.zeros((240,320),np.float32);full[13:226]=(depth-.4)*3*4
   small=np.rint(cv2.resize(full,(160,120),interpolation=cv2.INTER_AREA)).clip(-12,12).astype(np.int8)
   small=title_plane(small)
   raw=small.tobytes();packet=lz4.block.compress(raw,mode='high_compression',compression=9,store_size=False)
   if lz4.block.decompress(packet,uncompressed_size=19200)!=raw:raise ValueError('Depth mismatch')
   packets.append(packet)
   if i%100==0:print('Title depth',phase,i,flush=True)
  n=len(packets);offset=16+4*(n+1);offsets=[offset]
  for packet in packets:offsets.append(offsets[-1]+len(packet))
  path=dest/('title_'+phase+'.tsf');tmp=path.with_suffix('.tsf.part')
  with tmp.open('wb') as f:
   f.write(struct.pack('<4sIII',b'TSF1',160,120,n));f.write(struct.pack('<%dI'%(n+1),*offsets))
   for packet in packets:f.write(packet)
  os.replace(tmp,path);report.append({'phase':phase,'frames':n,'bytes':path.stat().st_size,'source_sha256':hashlib.sha256(clip.read_bytes()).hexdigest()})
 metadata={'version':1,'logo_quarter_pixels':quarter_pixels(),'logo_policy_sha256':hashlib.sha256(POLICY.read_bytes()).hexdigest(),'model_sha256':hashlib.sha256(Path(model).read_bytes()).hexdigest(),'base_sha256':hashlib.sha256((rom/'title_touch_top.rgb').read_bytes()).hexdigest(),'phases':report}
 for row in report:
  phase=row['phase'];row['corner_sha256']=hashlib.sha256((rom/'detail'/('title_touch_'+phase+'.lta')).read_bytes()).hexdigest()
  row['depth_sha256']=hashlib.sha256((dest/('title_'+phase+'.tsf')).read_bytes()).hexdigest()
 (dest/'title_stereo.json').write_text(json.dumps(metadata,indent=2),encoding='utf-8')
 return report
if __name__=='__main__':
 import sys
 root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(Path(__file__).parent))
 print(json.dumps(produce(root/'romfs',root/'tools/exe-production/model/depth-small.onnx',root/'work/title_stereo'),indent=2))
