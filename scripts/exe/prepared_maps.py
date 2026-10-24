"""Reconstruct stereo from committed geometry; no depth inference or matching."""
from pathlib import Path
import json,struct,shutil,sys
import numpy as np
import lz4.block
from state import sha,write_json

class VideoMaps:
 def __init__(self,path):
  self.f=Path(path).open('rb');magic,self.w,self.h,self.fps,self.n,self.group=struct.unpack('<4sHHIII',self.f.read(20))
  if magic!=b'LDP1' or (self.w,self.h,self.fps)!=(320,240,25):raise ValueError('Prepared video map format')
  self.offsets=struct.unpack('<%dI'%(self.n+1),self.f.read(4*(self.n+1)));self.cache={}
  if self.offsets[0]!=20+4*(self.n+1) or self.offsets[-1]!=Path(path).stat().st_size:raise ValueError('Prepared video index')
 def packet(self,index):
  self.f.seek(self.offsets[index]);data=self.f.read(self.offsets[index+1]-self.offsets[index]);first,count,mode,length=struct.unpack_from('<IHBI',data)
  if not first<=index<first+count or first+count>self.n or mode>7 or len(data)!=11+length:raise ValueError('Prepared video packet')
  return first,mode,data[11:]
 def flow(self,index):
  from mpeg_video import decode_depth
  if not 0<=index<self.n:raise ValueError('Prepared video frame out of range')
  if index in self.cache:return self.cache[index]
  first,_,_=self.packet(index);previous=None
  for frame in range(first,index+1):
   _,mode,body=self.packet(frame);previous=decode_depth(mode,body,previous)
  value=np.frombuffer(previous,np.int8).reshape(240,320).astype(np.float32)/4
  self.cache[index]=value
  for key in list(self.cache):
   if abs(key-index)>2:self.cache.pop(key)
  return value

def warp(left,depth):
 h,w=left.shape[:2]
 if depth.shape!=(h,w):raise ValueError('Geometry/image registration mismatch')
 y,x=np.mgrid[:h,:w];q=np.clip(x*4+depth.astype(np.int32),0,(w-1)*4);a=q//4;t=(q%4)[:,:,None]
 return ((left[y,a].astype(np.uint16)*(4-t)+left[y,np.minimum(a+1,w-1)].astype(np.uint16)*t+2)//4).astype(np.uint8)

def validate(root,maps):
 sys.path.insert(0,str(Path(root)/'release'));from export_maps import prepared_files
 maps=Path(maps).resolve()
 if maps!=Path(root).resolve()/'maps':raise ValueError('Use the reviewed maps directory in this checkout')
 prepared_files(root)
 return maps

def stills(rom,maps):
 rom,maps=Path(rom),Path(maps);stereo=rom/'stereo';stereo.mkdir(exist_ok=True)
 paths=list(rom.glob('clue_*.rgb'))+list(rom.glob('world_pantry_*.rgb'))+[rom/(n+'.rgb') for n in ['wires','piano','solar','keys','cupboard','map','door','solar_pattern']]+list(rom.glob('cinema_bg[0-9]*.rgb'))
 for path in paths:
  if path.stem.endswith('_R'):continue
  map_path=maps/(path.name+'.sdf')
  if not map_path.is_file():map_path=maps/'stereo'/(path.stem.removeprefix('clue_').removeprefix('world_')+'_L.rgb.sdf')
  packet=map_path.read_bytes();magic,w,h=struct.unpack_from('<4sHH',packet)
  if magic!=b'SDF1':raise ValueError('Missing prepared still map: '+path.name)
  left=np.frombuffer(path.read_bytes(),np.uint8).reshape(h,w,3);depth=np.frombuffer(lz4.block.decompress(packet[8:],uncompressed_size=w*h),np.int8).reshape(h,w);right=warp(left,depth)
  if path.stem.startswith('cinema_bg'):(rom/(path.stem+'_R.rgb')).write_bytes(right.tobytes())
  else:
   name=path.stem.removeprefix('clue_').removeprefix('world_');(stereo/(name+'_L.rgb')).write_bytes(left.tobytes());(stereo/(name+'_R.rgb')).write_bytes(right.tobytes())
 # Endpoint map already includes the authored logo plane.
 left=np.frombuffer((rom/'ending_L.rgb').read_bytes(),np.uint8).reshape(240,320,3)
 packet=(maps/'ending_L.rgb.sdf').read_bytes();depth=np.frombuffer(lz4.block.decompress(packet[8:],uncompressed_size=76800),np.int8).reshape(240,320)
 (rom/'ending_R.rgb').write_bytes(warp(left,depth).tobytes())
 for path in maps.rglob('*'):
  if path.suffix in {'.sdf','.depth','.tsf'}:
   target=rom/path.relative_to(maps);target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,target)
 return {'prepared_geometry':True,'depth_inference':False,'stereo_matching':False}

def title_metadata(rom,maps):
 from logo_depth import POLICY,quarter_pixels
 rom=Path(rom);phases=[]
 for phase in ['entry','idle']:
  path=rom/('title_'+phase+'.tsf');magic,w,h,n=struct.unpack('<4sIII',path.read_bytes()[:16])
  if magic!=b'TSF1':raise ValueError('Prepared title map')
  phases.append({'phase':phase,'frames':n,'source_sha256':sha(rom/('title_'+phase+'.lan')),'corner_sha256':sha(rom/'detail'/('title_touch_'+phase+'.lta')),'depth_sha256':sha(path)})
 write_json(rom/'title_stereo.json',{'version':1,'logo_policy_sha256':sha(POLICY),'logo_quarter_pixels':quarter_pixels(),'base_sha256':sha(rom/'title_touch_top.rgb'),'prepared_maps_sha256':sha(Path(maps)/'manifest.json'),'phases':phases})
 return phases

def videos(tasks,root,recovered,ffmpeg,maps):
 import mpeg_video
 from video import IDS
 maps=Path(maps);cache=Path(root)/'tools/exe-production';config=mpeg_video.settings(root)
 def plan(stage):
  cuts,details=mpeg_video.cut_plan(Path(recovered)/'Lume.swf',stage);write_json(stage/'plan.json',{'cuts':cuts,'source_intervals':details})
 plan_dir=tasks.run('prepared-video-cut-plan',{'swf':sha(Path(recovered)/'Lume.swf'),'planner':sha(Path(mpeg_video.__file__))},cache/'prepared-cut-plan',plan)
 cuts=json.loads((plan_dir/'plan.json').read_text())['cuts']
 for cid in IDS:
  source=Path(recovered)/'video'/(cid+'.flv');geometry=maps/'video'/('media_'+cid+'.ldp')
  def make(stage,source=source,geometry=geometry,cid=cid):
   depth=VideoMaps(geometry)
   mpeg_video.encode(source,None,stage/('media_'+cid+'.lvs'),ffmpeg,config,cuts[cid],depth_source=depth)
  tasks.run('prepared-video-'+cid,{'original':sha(source),'geometry':sha(geometry),'code':sha(Path(__file__)),'codec':sha(Path(mpeg_video.__file__)),'ffmpeg':sha(ffmpeg),'settings':config,'cuts':cuts[cid]},cache/'prepared-video'/cid,make)
 return cache/'prepared-video'