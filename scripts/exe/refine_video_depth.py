"""Replace only LVM1 disparity packets; preserve every encoded colour byte."""
from pathlib import Path
import hashlib,json,struct
import numpy as np
from mpeg_video import header,packet,depth_packet,validate_file,PACKET,MAGIC
from stereo_resample import StereoSource

def refine(path,stereo_path,dest):
 path=Path(path);dest=Path(dest)
 if path.resolve()==dest.resolve():raise ValueError('Use a separate candidate output')
 n,group,ix=header(path);source=StereoSource(stereo_path)
 rows=[];previous=None;colour_hash=hashlib.sha256();raw_hash=hashlib.sha256()
 try:
  with path.open('rb') as f:
   for index in range(n):
    first,count,flags,colour,old_depth=packet(f,ix,index)
    if first==index:previous=None
    pos=index*source.fps/25;low=int(pos);fraction=pos-low
    flow=source.flow(low)*(1-fraction)+source.flow(low+1)*fraction
    raw=np.rint(flow*4).clip(-128,127).astype(np.int8).tobytes()
    mode,depth=depth_packet(raw,previous);previous=raw
    rows.append(PACKET.pack(first,count,(flags&256)|mode,len(colour),len(depth))+colour+depth)
    colour_hash.update(colour);raw_hash.update(raw)
  offsets=[20+4*(n+1)]
  for row in rows:offsets.append(offsets[-1]+len(row))
  dest.parent.mkdir(parents=True,exist_ok=True)
  dest.write_bytes(struct.pack('<4sHHIII',MAGIC,320,240,25,n,group)+struct.pack('<%dI'%(n+1),*offsets)+b''.join(rows))
  if validate_file(dest)!=n:raise ValueError('Candidate validation failed')
  after=hashlib.sha256();_,_,out_ix=header(dest)
  with dest.open('rb') as f:
   for i in range(n):after.update(packet(f,out_ix,i)[3])
  if after.digest()!=colour_hash.digest():raise ValueError('Colour payload changed')
  return {'frames':n,'colour_payload_sha256':after.hexdigest(),'colour_unchanged':True,
          'depth_sha256':raw_hash.hexdigest(),'before_bytes':path.stat().st_size,'after_bytes':dest.stat().st_size}
 finally:source.f.close()
