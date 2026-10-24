"""Original colour + recovered stereo displacement, without old-media inputs.

The initial neural conversion deliberately retains the recovered15Hz method.
Only displacement is interpolated; original colour is decoded anew at25Hz.
This preserves the historical pipeline while making every input reproducible.
"""
from pathlib import Path
import os,struct,subprocess,shutil

IDS=('0175','0740','0940','0945')
MODEL_SHA='afb6a5c28f3b6bf1618c6e43f02073ef9dfdc70e937502d51603e57b0a1df10c'
MODEL_URL='https://huggingface.co/onnx-community/depth-anything-v2-small/resolve/main/onnx/model.onnx'

def read_exact(stream,size):
 out=bytearray()
 while len(out)<size:
  block=stream.read(size-len(out))
  if not block:break
  out.extend(block)
 if out and len(out)!=size:raise ValueError('FFmpeg returned an incomplete frame.')
 return bytes(out)

def encode(src,stereo_path,dest,ffmpeg):
 """Legacy LVS2 producer; full EXE production uses mpeg_video.encode."""
 import numpy as np
 import lz4.block
 from stereo_resample import StereoSource
 stereo=StereoSource(stereo_path);offsets=[];dest=Path(dest);spool=dest.with_suffix('.spool')
 process=subprocess.Popen([str(ffmpeg),'-v','error','-nostdin','-i',str(src),'-vf','fps=25,scale=320:213,format=rgb24,pad=320:240:0:13','-f','rawvideo','-pix_fmt','rgb24','pipe:1'],stdout=subprocess.PIPE)
 count=0;previous=None;delta_count=0
 try:
  with spool.open('wb') as out:
   while data:=read_exact(process.stdout,230400):
    rgb=np.frombuffer(data,np.uint8).reshape(240,320,3).astype(np.uint16)
    colour=(((rgb[:,:,0]*31+127)//255)<<11)|(((rgb[:,:,1]*63+127)//255)<<5)|((rgb[:,:,2]*31+127)//255)
    pos=count*stereo.fps/25;low=int(pos);fraction=pos-low
    flow=stereo.flow(low)*(1-fraction)+stereo.flow(low+1)*fraction
    disparity=np.rint(flow*4).clip(-128,127).astype(np.int8)
    raw=colour.astype('<u2').tobytes()+disparity.tobytes()
    packet=b'\0'+lz4.block.compress(raw,mode='high_compression',compression=9,store_size=False)
    if count%4 and previous is not None:
     values=np.frombuffer(raw,np.uint8);old=np.frombuffer(previous,np.uint8)
     residual=np.concatenate([(values[:153600:2]-old[:153600:2]),(values[1:153600:2]-old[1:153600:2]),values[153600:]]).tobytes()
     candidate=b'\1'+lz4.block.compress(residual,mode='high_compression',compression=9,store_size=False)
     if len(candidate)<len(packet):packet=candidate;delta_count+=1
    check=lz4.block.decompress(packet[1:],uncompressed_size=230400)
    if packet[0]:
     values=np.frombuffer(check,np.uint8);old=np.frombuffer(previous,np.uint8);restored=np.empty(230400,np.uint8)
     restored[:153600:2]=values[:76800]+old[:153600:2];restored[1:153600:2]=values[76800:153600]+old[1:153600:2];restored[153600:]=values[153600:];check=restored.tobytes()
    if check!=raw:raise ValueError('LZ4 video verification failed.')
    previous=raw
    offsets.append(out.tell());out.write(packet);count+=1
    if count%100==0:print('LVS',src.stem,count,flush=True)
   end=out.tell()
  if process.wait() or not count:raise ValueError('Source video decoding failed: '+str(src))
  header=20+4*(count+1)
  with dest.open('wb') as out,spool.open('rb') as data:
   out.write(struct.pack('<4sHHIII',b'LVS2',320,240,25,count,4))
   out.write(struct.pack('<%dI'%(count+1),*[header+x for x in offsets+[end]]));shutil.copyfileobj(data,out)
  return count
 finally:
  if process.poll() is None:process.kill();process.wait()
  process.stdout.close();stereo.f.close();spool.unlink(missing_ok=True)

def produce(tasks,root,recovered,ffmpeg,force_model=None):
 import sys
 sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
 import toolchain as cv
 from state import sha,write_json
 from stereo_depth import Stereo,video
 root=Path(root);cache=root/'tools/exe-production'
 model=Path(force_model).resolve() if force_model else cv.download(MODEL_URL,cache/'model/depth-small.onnx',MODEL_SHA)
 if sha(model)!=MODEL_SHA:raise ValueError('Depth model SHA256 mismatch.')
 # The recovered generator invokes ffmpeg by basename. Its PATH is scoped to
 # this process, without a permanent system PATH modification.
 os.environ['PATH']=str(Path(ffmpeg).parent)+os.pathsep+os.environ.get('PATH','')
 os.environ['LUME_FFMPEG']=str(ffmpeg)
 import mpeg_video
 config=mpeg_video.settings(root)
 def plan(stage):
  cuts,details=mpeg_video.cut_plan(Path(recovered)/'Lume.swf',stage)
  write_json(stage/'plan.json',{'cuts':cuts,'source_intervals':details})
 plan_dir=tasks.run('video-cut-plan',{'swf':sha(Path(recovered)/'Lume.swf'),'recipe':sha(Path(__file__).with_name('recipe.py')),'planner':sha(Path(__file__).with_name('mpeg_video.py'))},cache/'video-cut-plan',plan)
 cuts=__import__('json').loads((plan_dir/'plan.json').read_text(encoding='utf-8'))['cuts']
 for cid in IDS:
  src=Path(recovered)/'video'/(cid+'.flv');inputs={'source':sha(src),'model':MODEL_SHA,'stereo_code':sha(Path(__file__).with_name('stereo_depth.py')),'ffmpeg':sha(ffmpeg),'fps':15}
  def make_pair(stage,src=src,cid=cid):
   video(Stereo(model),src,stage/(cid+'.ljv'),15)
  pairs=tasks.run('stereo-'+cid,inputs,cache/'stereo'/cid,make_pair)
  def make_lvs(stage,src=src,cid=cid,pairs=pairs):
   mpeg_video.encode(src,pairs/(cid+'.ljv'),stage/('media_'+cid+'.lvs'),ffmpeg,config,cuts[cid])
  tasks.run('video-'+cid,{'source':sha(src),'stereo':sha(pairs/(cid+'.ljv')),'code':sha(Path(__file__)),'codec':sha(Path(__file__).with_name('mpeg_video.py')),'flow_code':sha(Path(__file__).with_name('stereo_resample.py')),'settings':config,'cuts':cuts[cid],'ffmpeg':sha(ffmpeg)},cache/'video'/cid,make_lvs)
