"""Inspect stored stereo stages, or reproduce neural intermediates locally.

Inspection never modifies game assets or production checkpoints. A final LVS
stores signed disparity, not the original neural depth: keep these distinct.
"""
from pathlib import Path
import argparse, html, io, json, os, struct, subprocess, sys
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE));sys.path.insert(0,str(HERE.parent))
import numpy as np
from PIL import Image
from state import sha,write_json
from video import IDS,read_exact


def ljv_frame(path,seconds):
 with path.open('rb') as f:
  magic,w,h,fps,n,_=struct.unpack('<4sHHIII',f.read(20))
  if magic!=b'LJV1' or not fps or not n:raise ValueError('Invalid LJV header')
  ix=struct.unpack('<%dI'%(n+1),f.read(4*(n+1)));index=min(n-1,max(0,int(seconds*fps+.5)))
  f.seek(ix[index]);packet=f.read(ix[index+1]-ix[index]);a,b=struct.unpack_from('<II',packet)
  if a+b+8!=len(packet):raise ValueError('Invalid LJV packet')
  return index,Image.open(io.BytesIO(packet[8:8+a])).convert('RGB'),Image.open(io.BytesIO(packet[8+a:])).convert('RGB')


def lvs_info(path):
 with path.open('rb') as f:
  magic,w,h,fps,n,key=struct.unpack('<4sHHIII',f.read(20))
 if magic not in (b'LVS1',b'LVS2',b'LVM1') or (w,h)!=(320,240) or not fps or not n or (magic==b'LVS2' and not 1<=key<=256):raise ValueError('Invalid LVS header')
 return magic,w,h,fps,n,key


def lvs_frame(path,index):
 import lz4.block
 magic,w,h,fps,n,key=lvs_info(path);index=min(n-1,max(0,index));size=w*h
 if magic==b'LVM1':
  from mpeg_video import read_frame,resolve_ffmpeg
  return read_frame(path,index,resolve_ffmpeg(path))
 with path.open('rb') as f:
  f.seek(20);ix=struct.unpack('<%dI'%(n+1),f.read(4*(n+1)));previous=None
  for fr in range(index-index%key if magic==b'LVS2' else index,index+1):
   f.seek(ix[fr]);packet=f.read(ix[fr+1]-ix[fr]);mode=packet[0] if magic==b'LVS2' else 0
   raw=lz4.block.decompress(packet[1:] if magic==b'LVS2' else packet,uncompressed_size=size*3)
   if mode not in (0,1) or len(raw)!=size*3:raise ValueError('Invalid LVS packet')
   if mode:
    if previous is None:raise ValueError('Delta without keyframe')
    a=np.frombuffer(raw,np.uint8);b=np.frombuffer(previous,np.uint8);v=np.empty(size*3,np.uint8)
    v[:size*2:2]=a[:size]+b[:size*2:2];v[1:size*2:2]=a[size:size*2]+b[1:size*2:2];v[size*2:]=a[size*2:];raw=v.tobytes()
   previous=raw
 colour=np.frombuffer(raw[:size*2],'<u2').reshape(h,w);r,g,b=colour>>11,(colour>>5)&63,colour&31
 left=np.stack([(r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2)],axis=2).astype(np.uint8)
 disparity=np.frombuffer(raw[size*2:],np.int8).reshape(h,w).copy()
 yy,xx=np.mgrid[:h,:w];q=(xx*4+disparity).clip(0,(w-1)*4);sx=q>>2;frac=(q&3)[:,:,None]
 right=((left[yy,sx].astype(np.uint16)*(4-frac)+left[yy,np.minimum(sx+1,w-1)].astype(np.uint16)*frac+2)>>2).astype(np.uint8)
 return Image.fromarray(left),Image.fromarray(right),disparity


def save_pair(folder,left,right):
 folder.mkdir(parents=True,exist_ok=True)
 if not isinstance(left,Image.Image):left=Image.fromarray(left)
 if not isinstance(right,Image.Image):right=Image.fromarray(right)
 left.save(folder/'left.png');right.save(folder/'right.png')
 sbs=Image.new('RGB',(left.width*2,left.height));sbs.paste(left,(0,0));sbs.paste(right,(left.width,0));sbs.save(folder/'SBS.png')


def gallery(out):
 rows=[]
 for p in sorted(out.rglob('SBS.png')):
  rel=p.relative_to(out).as_posix();label=html.escape(str(p.parent.relative_to(out)))
  rows.append('<figure><figcaption>'+label+'</figcaption><a href="'+html.escape(rel)+'"><img loading="lazy" src="'+html.escape(rel)+'"></a></figure>')
 (out/'index.html').write_text('<!doctype html><meta charset="utf-8"><title>Stereo previews</title><style>body{background:#202124;color:#eee;font:16px sans-serif}img{max-width:100%;image-rendering:auto}figure{margin:24px 0}a{color:#9cf}</style><h1>Stereo previews</h1><p>Left/right eyes. Each folder includes PNG images, numerical maps and method records. Final disparity differs from neural depth.</p>'+''.join(rows),encoding='utf-8')


def find_ffmpeg(root,explicit):
 if explicit:return Path(explicit).resolve()
 for base in (root/'tools/detail/ffmpeg',root/'tools'):
  hits=list(base.rglob('ffmpeg.exe')) if base.exists() else []
  if hits:return hits[0].resolve()
 raise ValueError('FFmpeg not found; specify its path with --ffmpeg.')


def trace(root,cid,out,ffmpeg,model,every,strength):
 # Replay all 15 Hz frames so the 75/25 temporal history is the same as the
 # production neural stage. Only sampled PNG/NPY files are retained.
 from stereo_depth import Stereo
 src=root/'tools/exe-production/recovered/video'/(cid+'.flv')
 if not src.is_file():raise ValueError('Kaynak FLV yok: '+str(src))
 stereo=Stereo(model,strength=strength);model_hash=sha(model);step=max(1,round(every*15));i=0
 cmd=[str(ffmpeg),'-v','error','-nostdin','-i',str(src),'-vf','fps=15:eof_action=pass,scale=320:213,format=rgb24,pad=320:240:0:13','-f','rawvideo','-pix_fmt','rgb24','pipe:1']
 p=subprocess.Popen(cmd,stdout=subprocess.PIPE)
 try:
  while data:=read_exact(p.stdout,320*240*3):
   rgb=np.frombuffer(data,np.uint8).reshape(240,320,3);left,right,depth=stereo.pair(rgb,True)
   if i%step==0:
    dest=out/cid/'neural'/('%06d'%i);save_pair(dest,left,right)
    np.save(dest/'depth.npy',depth);Image.fromarray(np.rint(depth.clip(0,1)*65535).astype(np.uint16)).save(dest/'depth16.png')
    Image.fromarray(np.rint(depth.clip(0,1)*255).astype(np.uint8)).save(dest/'depth_view.png')
    write_json(dest/'method.json',{'frame':i,'fps':15,'seconds':i/15,'model_sha256':model_hash,'strength':strength,'temporal':'75 percent current, 25 percent previous; sequential replay','stage':'before JPEG encoding / Farneback / 25 Hz resampling','matches_default_production_settings':strength==5})
    print(cid,'depth',i,flush=True)
   i+=1
  if p.wait() or not i:raise ValueError('FFmpeg kaynak kareleri okuyamadi.')
 finally:
  if p.poll() is None:p.kill();p.wait()
  p.stdout.close()


def alternate(a,out):
 # A custom depth image is resampled to the source view and normalized by its
 # integer encoding range, not by its extrema, so authored depth values survive.
 from stereo_depth import Stereo
 rgb=np.asarray(Image.open(a.input).convert('RGB'));h,w=rgb.shape[:2]
 if a.depth_image:
  image=Image.open(a.depth_image);raw=np.asarray(image)
  if raw.ndim!=2:raise ValueError('Depth resmi tek kanalli olmali.')
  maximum=65535 if image.mode.startswith('I;16') or (image.mode=='I' and raw.max()>255) else 255
  depth=np.asarray(image.resize((w,h),Image.Resampling.BILINEAR),np.float32)/maximum
  depth=depth.clip(0,1);stereo=object.__new__(Stereo);stereo.strength=a.strength
  stereo.depth=lambda image,temporal=False:depth
 else:
  if not a.model:raise ValueError('--model veya --depth-image gerekli.')
  stereo=Stereo(a.model,strength=a.strength)
 left,right,depth=stereo.pair(rgb);dest=out/'alternative';save_pair(dest,left,right);np.save(dest/'depth.npy',depth)
 Image.fromarray(np.rint(depth.clip(0,1)*65535).astype(np.uint16)).save(dest/'depth16.png')
 write_json(dest/'method.json',{'input':str(a.input),'depth_image':str(a.depth_image) if a.depth_image else None,'model_sha256':sha(a.model) if a.model and not a.depth_image else None,'strength':a.strength,'stage':'single-image alternative preview; no production assets changed'})


def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--root',type=Path,default=HERE.parents[1]);ap.add_argument('--out',type=Path)
 ap.add_argument('--movie',choices=IDS);ap.add_argument('--every-seconds',type=float,default=10);ap.add_argument('--at',type=float)
 ap.add_argument('--trace',action='store_true');ap.add_argument('--model',type=Path);ap.add_argument('--ffmpeg');ap.add_argument('--strength',type=float,default=5)
 ap.add_argument('--input',type=Path);ap.add_argument('--depth-image',type=Path);a=ap.parse_args()
 if a.every_seconds<=0 or a.at is not None and a.at<0:ap.error('Zaman degerleri pozitif olmali.')
 root=a.root.resolve();out=(a.out or root/'previews/stereo').resolve();out.mkdir(parents=True,exist_ok=True)
 with __import__('state').Lock(root/'tools/exe-production/production.lock'):
  if a.input:alternate(a,out)
  elif a.trace:
   model=a.model or root/'tools/exe-production/model/depth-small.onnx'
   if not model.is_file():raise ValueError('Model yok; --model yolunu belirtin.')
   ffmpeg=find_ffmpeg(root,a.ffmpeg)
   for cid in [a.movie] if a.movie else IDS:trace(root,cid,out,ffmpeg,model,a.every_seconds,a.strength)
  else:
   found=False
   for cid in [a.movie] if a.movie else IDS:
    cached=root/'tools/exe-production/video'/cid/('media_'+cid+'.lvs');published=root/'romfs'/('media_'+cid+'.lvs')
    movie=cached if cached.is_file() else published
    if not movie.is_file():print('Atlandi, LVS yok:',cid);continue
    found=True;magic,w,h,fps,n,key=lvs_info(movie)
    indices=[min(n-1,int(a.at*fps+.5))] if a.at is not None else sorted(set(range(0,n,max(1,round(a.every_seconds*fps))))|{n-1})
    pairs=root/'tools/exe-production/stereo'/cid/(cid+'.ljv')
    for i in indices:
     dest=out/cid/('%06d'%i);left,right,disparity=lvs_frame(movie,i);save_pair(dest/'final',left,right)
     np.save(dest/'final/disparity_quarter_pixel.npy',disparity)
     Image.fromarray((disparity.astype(np.int16)+128).astype(np.uint8)).save(dest/'final/disparity_view.png')
     metadata={'frame':i,'fps':fps,'seconds':i/fps,'final_source':str(movie),'disparity_units':'signed quarter pixels; PNG visualization adds 128, neutral=128','depth':'neural depth not stored in LVS; use --trace for sequential reconstruction'}
     if pairs.is_file():
      j,l,r=ljv_frame(pairs,i/fps);save_pair(dest/'intermediate_ljv',l,r);metadata['intermediate_frame']=j;metadata['intermediate_fps']=15
     write_json(dest/'method.json',metadata);print(cid,i,flush=True)
   if not found:raise ValueError('No generated LVS found. Complete production first.')
 gallery(out);print('Goruntuler:',out/'index.html')


if __name__=='__main__':
 try:main()
 except Exception as e:print('ERROR:',e);raise SystemExit(1)
