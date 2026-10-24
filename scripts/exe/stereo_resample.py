"""Offline 25 fps source video with disparity transferred from existing stereo pairs.
No models, accounts, network calls or telemetry. The original left view is decoded
at its native cadence; only the small right-eye displacement is interpolated.
"""
from pathlib import Path
import io,struct,subprocess,json
import numpy as np
import cv2
from PIL import Image
def refine_edges(left,right,flow):
 """Offline rectified edge matching; keep source order to prevent double edges.
 Farneback's large window bends thin high-contrast silhouettes. SGBM supplies
 a bounded correction near those edges; a frame-level photometric gate rejects
 a worse candidate. No matching runs on the console.
 """
 gray=cv2.cvtColor(right,cv2.COLOR_RGB2GRAY)
 mask=cv2.dilate(cv2.Canny(gray,45,100),np.ones((7,7),np.uint8))>0
 mask[:13]=False;mask[226:]=False
 if not np.any(mask):return flow
 matcher=cv2.StereoSGBM_create(minDisparity=-32,numDisparities=64,blockSize=3,
  P1=216,P2=864,uniquenessRatio=5,speckleWindowSize=0,disp12MaxDiff=2,
  mode=cv2.STEREO_SGBM_MODE_SGBM_3WAY)
 matched=-matcher.compute(right,left).astype(np.float32)/16
 valid=(matched<32)&(np.abs(matched-flow)<12)
 candidate=np.where(valid&mask,matched,flow)
 yy,xx=np.mgrid[:flow.shape[0],:flow.shape[1]].astype(np.float32)
 # Inverse sampling must not reverse source order: that repeats the sky
 # inside the roof and creates a second bright, serrated silhouette.
 candidate=np.maximum.accumulate(xx+candidate,axis=1)-xx
 candidate=np.clip(np.rint(candidate*4),-128,127)/4
 candidate[:13]=0;candidate[226:]=0
 def error(field):
  warped=cv2.remap(left,xx+field,yy,cv2.INTER_LINEAR,borderMode=cv2.BORDER_REPLICATE)
  return np.mean(np.abs(warped.astype(np.float32)-right)[mask])
 return candidate.astype(np.float32) if error(candidate)<error(flow) else flow

class StereoSource:
 def __init__(self,path):
  self.f=Path(path).open('rb');h=self.f.read(20);_,self.w,self.h,self.fps,self.n,_=struct.unpack('<4sHHIII',h);self.offsets=struct.unpack('<'+'I'*(self.n+1),self.f.read(4*(self.n+1)));self.cache={}
 def flow(self,index):
  index=min(max(index,0),self.n-1)
  if index in self.cache:return self.cache[index]
  self.f.seek(self.offsets[index]);l,r=struct.unpack('<II',self.f.read(8));left=np.asarray(Image.open(io.BytesIO(self.f.read(l))).convert('RGB'));right=np.asarray(Image.open(io.BytesIO(self.f.read(r))).convert('RGB'))
  f=cv2.calcOpticalFlowFarneback(cv2.cvtColor(right,cv2.COLOR_RGB2GRAY),cv2.cvtColor(left,cv2.COLOR_RGB2GRAY),None,.5,3,19,4,5,1.1,0)[:,:,0];f=np.clip(f,-31.75,31.75);f[:13]=0;f[226:]=0
  f=refine_edges(left,right,f);self.cache[index]=f
  for k in list(self.cache):
   if abs(k-index)>2:self.cache.pop(k)
  return f

def encode(rgb):
 out=io.BytesIO();Image.fromarray(rgb).save(out,format='JPEG',quality=72,subsampling=2);return out.getvalue()
def convert(src,old,dest):
 stereo=StereoSource(old);cmd=['ffmpeg','-v','error','-nostdin','-i',str(src),'-vf','fps=25,scale=320:213,format=rgb24,pad=320:240:0:13','-f','rawvideo','-pix_fmt','rgb24','pipe:1'];p=subprocess.Popen(cmd,stdout=subprocess.PIPE)
 spool=dest.with_suffix('.spool');offsets=[];yy,xx=np.mgrid[:240,:320].astype(np.float32);count=0
 with spool.open('wb')as f:
  while True:
   data=p.stdout.read(320*240*3)
   if not data:break
   assert len(data)==320*240*3;left=np.frombuffer(data,np.uint8).reshape(240,320,3)
   pos=count*stereo.fps/25;low=int(pos);frac=pos-low;flow=stereo.flow(low)*(1-frac)+stereo.flow(low+1)*frac
   right=cv2.remap(left,xx+flow,yy,cv2.INTER_LINEAR,borderMode=cv2.BORDER_REPLICATE)
   l,r=encode(left),encode(right);offsets.append(f.tell());f.write(struct.pack('<II',len(l),len(r))+l+r);count+=1
 assert p.wait()==0 and count>0;size=spool.stat().st_size;header=20+4*(count+1)
 with dest.open('wb')as f,spool.open('rb')as inp:
  f.write(struct.pack('<4sHHIII',b'LJV1',320,240,25,count,0));f.write(struct.pack('<'+'I'*(count+1),*[header+x for x in offsets+[size]]));__import__('shutil').copyfileobj(inp,f)
 spool.unlink();stereo.f.close();print(dest.name,count,'native source frames',dest.stat().st_size,flush=True);return count
if __name__=='__main__':
 root=Path(__file__).resolve().parents[1];repo=root.parents[1];out=repo/'qa-v6/media-offline25';out.mkdir(exist_ok=True);counts={}
 for cid in ['0175','0740','0940','0945']:counts[cid]=convert(root/'recovered/video'/(cid+'.flv'),repo/'project/SD_Kart/3ds/lume_puzzles/media'/(cid+'.ljv'),out/(cid+'.ljv'))
 (root/'assets/stereo-v6.json').write_text(json.dumps({'fps':25,'method':'native source left view; locally estimated disparity from prior stereo pair sequence, interpolated at source cadence','frames':counts,'jpeg_subsampling':'4:2:0','network':False},indent=2), encoding="utf-8")
