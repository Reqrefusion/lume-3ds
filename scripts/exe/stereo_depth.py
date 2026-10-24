#!/usr/bin/env python3
"""Depth-Anything-V2 stereo reprojection. Left eye remains the original view."""
import argparse,hashlib,io,json,struct,subprocess,time
from pathlib import Path
import numpy as np
from PIL import Image
import cv2,onnxruntime as ort

import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'source/engine/tools'))
from depth_estimator import Stereo

def jpeg(image):
    buffer=io.BytesIO();Image.fromarray(image).save(buffer,format='JPEG',quality=72,subsampling=0);return buffer.getvalue()
def video(stereo,src,dest,fps):
    cmd=['ffmpeg','-v','error','-nostdin','-i',str(src),'-vf',f'fps={fps}:eof_action=pass,scale=320:213,format=rgb24,pad=320:240:0:13','-f','rawvideo','-pix_fmt','rgb24','pipe:1']
    process=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL);frames=[];stereo.previous=None;start=time.monotonic()
    # Temporary frame spool avoids retaining the complete movie in memory.
    spool=dest.with_suffix('.frames');spool.parent.mkdir(parents=True,exist_ok=True)
    try:
        with spool.open('wb') as out:
            while True:
                data=process.stdout.read(320*240*3)
                if not data:break
                if len(data)!=320*240*3:raise ValueError('Truncated FFmpeg frame')
                left,right,_=stereo.pair(np.frombuffer(data,np.uint8).reshape(240,320,3),True);l,r=jpeg(left),jpeg(right)
                frames.append(out.tell());out.write(struct.pack('<II',len(l),len(r))+l+r)
                if len(frames)%100==0:print(src.name,len(frames),'frames',round(time.monotonic()-start),'sec',flush=True)
        if process.wait()!=0 or not frames:raise RuntimeError('Decode failed: '+str(src))
        payload_size=spool.stat().st_size;header_size=20+4*(len(frames)+1)
        with dest.open('wb') as out,spool.open('rb') as inp:
            out.write(struct.pack('<4sHHIII',b'LJV1',320,240,fps,len(frames),0));out.write(struct.pack('<'+'I'*(len(frames)+1),*(header_size+x for x in frames+[payload_size])))
            import shutil;shutil.copyfileobj(inp,out)
        print('Finished',dest.name,len(frames),'frames',dest.stat().st_size,'bytes',flush=True)
        return {'file':dest.name,'frames':len(frames),'fps':fps,'bytes':dest.stat().st_size}
    finally:
        if process.poll() is None:process.kill();process.wait()
        process.stdout.close();spool.unlink(missing_ok=True)

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--model',type=Path,required=True);ap.add_argument('--root',type=Path,required=True);ap.add_argument('--videos',action='store_true');ap.add_argument('--fps',type=int,default=15);a=ap.parse_args()
    root=a.root;stereo=Stereo(a.model);out=root/'native/romfs/stereo';out.mkdir(parents=True,exist_ok=True);metadata={'model':'onnx-community/depth-anything-v2-small','model_sha256':hashlib.sha256(a.model.read_bytes()).hexdigest(),'method':'depth-dependent forward reprojection with nearest-depth occlusion handling and inpainted disocclusions','strength_pixels_at_320':5,'images':[],'videos':[]}
    if (root/'stereo-conversion.json').exists() and not a.videos:metadata['videos']=json.loads((root/'stereo-conversion.json').read_text(encoding="utf-8")).get('videos',[])
    sources=list((root/'assets/puzzles').glob('*.png'))+list((root/'assets/world').glob('*.png'))
    for p in sources:
        im=Image.open(p).convert('RGB');pattern=p.stem=='solar_pattern';im=im.resize((160,160) if pattern else (320,213),Image.Resampling.LANCZOS)
        if not pattern:
            canvas=Image.new('RGB',(320,240));canvas.paste(im,(0,13));im=canvas
        l,r,d=stereo.pair(np.asarray(im));(out/(p.stem+'_L.rgb')).write_bytes(l.tobytes());(out/(p.stem+'_R.rgb')).write_bytes(r.tobytes())
        preview=root/'previews/stereo';preview.mkdir(parents=True,exist_ok=True);buf=io.BytesIO();Image.fromarray(np.concatenate([l,r],axis=1)).save(buf,format='PNG');(preview/(p.stem+'_SBS.png')).write_bytes(buf.getvalue())
        metadata['images'].append({'name':p.stem,'mean_absolute_eye_difference':float(np.abs(l.astype(float)-r).mean())});print('Stereo image',p.stem,flush=True)
    if a.videos:
        for p in sorted((root/'recovered/video').glob('*.flv')):metadata['videos'].append(video(stereo,p,root/'media'/(p.stem+'.ljv'),a.fps))
    (root/'stereo-conversion.json').write_text(json.dumps(metadata,indent=2), encoding="utf-8")
if __name__=='__main__':main()
