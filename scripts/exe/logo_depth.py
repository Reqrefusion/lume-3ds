"""One authored logo plane shared by title fields and ending endpoints."""
from pathlib import Path
import re,struct,hashlib
POLICY=Path(__file__).resolve().parents[2]/'source/game/lume/logo_depth.h'
def quarter_pixels():
 return int(re.search(r'#define LUME_LOGO_QUARTER_PIXELS (\d+)',POLICY.read_text(encoding='utf-8')).group(1))
def title_plane(small):
 import numpy as np
 h,w=small.shape;y,x=np.mgrid[:h,:w];x=x*320/w;y=y*240/h
 # A flat plane covers the complete word; feathering happens outside ink.
 distance=np.maximum.reduce([70-x,x-254,34-y,y-105,np.zeros_like(x)])
 weight=np.clip(1-distance/8,0,1)
 return np.rint(small.astype(np.float32)*(1-weight)+quarter_pixels()*weight).clip(-128,127).astype(np.int8)
def ending(rom):
 import numpy as np,lz4.block
 from stereo_fields import raster
 from compress import static
 rom=Path(rom);left=raster(rom/'ending_L.rgb');packet=(rom/'ending_L.rgb.sdf').read_bytes()
 depth=np.frombuffer(lz4.block.decompress(packet[8:],uncompressed_size=76800),np.int8).reshape(240,320).copy()
 depth[18:62,102:218]=quarter_pixels()
 y,x=np.mgrid[:240,:320];q=np.clip(x*4+depth.astype(np.int32),0,319*4);a=q//4;b=np.minimum(a+1,319);t=q%4
 right=((left[y,a].astype(np.uint16)*(4-t[:,:,None])+left[y,b].astype(np.uint16)*t[:,:,None]+2)//4).astype(np.uint8)
 path=rom/'ending_R.rgb';path.write_bytes(right.tobytes());static(path)
 (rom/'ending_L.rgb.sdf').write_bytes(struct.pack('<4sHH',b'SDF1',320,240)+lz4.block.compress(depth.tobytes(),mode='high_compression',compression=9,store_size=False))
