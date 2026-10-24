"""Bounded MPEG colour groups plus exact, independently predicted disparity.

Quality decisions are recorded per source-frame interval. Bad colour groups
are retried at a finer quantizer, then stored as exact RGB if necessary.
"""
from pathlib import Path
import json,os,struct,subprocess,tempfile,shutil
import numpy as np
import cv2,lz4.block
from PIL import Image
from state import sha,write_json

MAGIC=b'LVM1';WIDTH=320;HEIGHT=240;FPS=25;PIXELS=WIDTH*HEIGHT
PACKET=struct.Struct('<IHHII');MAX_COLOUR=512*1024
FILTER='fps=25,scale=320:213:flags=lanczos,format=rgb24,pad=320:240:0:13:black'
LEGACY_DEFAULTS={'gop_frames':12,'quantizer':2,'retry_quantizer':1,'min_luma_ssim':0.985,'max_rgb_mae':3.0,'preserve_failed_groups_losslessly':True}
DEFAULTS={'profile':'balanced','gop_frames':12,'quantizer':3,'retry_quantizer':1,'min_luma_ssim':0.95,'max_rgb_mae':4.5,'preserve_failed_groups_losslessly':False}


def settings(root):
 p=Path(root)/'video-settings.json';cfg=dict(DEFAULTS)
 if p.is_file():
  supplied=json.loads(p.read_text(encoding='utf-8-sig'))
  if supplied==LEGACY_DEFAULTS:
   backup=p.with_name('video_ayarlari.kati-profil-yedegi.json')
   if not backup.exists():shutil.copy2(p,backup)
   write_json(p,cfg);print('Eski kati video varsayilanlari dengeli profile gecirildi.',flush=True)
  else:cfg.update(supplied)
 else:write_json(p,cfg)
 if not isinstance(cfg['gop_frames'],int) or isinstance(cfg['gop_frames'],bool) or not 1<=cfg['gop_frames']<=32:raise ValueError('gop_frames must be 1..32')
 for k in ['quantizer','retry_quantizer']:
  if not isinstance(cfg[k],int) or isinstance(cfg[k],bool) or not 1<=cfg[k]<=6:raise ValueError(k+' must be 1..6')
 if cfg['retry_quantizer']>cfg['quantizer']:raise ValueError('Retry quantizer cannot reduce quality')
 for k in ['min_luma_ssim','max_rgb_mae']:
  if isinstance(cfg[k],bool) or not isinstance(cfg[k],(float,int)) or not np.isfinite(cfg[k]):raise ValueError('Invalid '+k)
 if not 0<cfg['min_luma_ssim']<=1 or cfg['max_rgb_mae']<0:raise ValueError('Invalid colour quality limits')
 if not isinstance(cfg['preserve_failed_groups_losslessly'],bool):raise ValueError('Lossless fallback must be boolean')
 return cfg


def cut_plan(swf,work):
 from recipe import make
 from video import IDS
 req=Path(work)/'requests';make(swf,req);cuts={cid:{0} for cid in IDS};details=[]
 for p in sorted(req.glob('*.tsv')):
  if p.stem in ['plates','stops']:continue
  rows=[tuple(map(int,line.split('\t')[:5])) for line in p.read_text(encoding='utf-8').splitlines()[1:]]
  prior=None
  for root,child,minimum,mid,frame in rows:
   if mid>=4:
    if prior:cuts[IDS[prior[0]]].add(prior[1]+1)
    prior=None;continue
   if prior is None or mid!=prior[0] or frame not in (prior[1],prior[1]+1):
    if prior:cuts[IDS[prior[0]]].add(prior[1]+1)
    cuts[IDS[mid]].add(frame);details.append({'clip':p.stem,'movie':IDS[mid],'frame':frame,'root_frame':root})
   prior=(mid,frame)
  if prior:cuts[IDS[prior[0]]].add(prior[1]+1)
 # Stopped cameras are also legal cold seeks, independent of movement clips.
 for frame in [1602,974,260,377,501,797]:cuts['0175'].update([frame,frame+1])
 return {k:sorted(v) for k,v in cuts.items()},details


def colour_args(cfg,quantizer):
 return ['-an','-c:v','mpeg1video','-q:v',str(quantizer),'-qmin',str(quantizer),'-qmax',str(quantizer),'-bf','0','-g',str(cfg['gop_frames']),'-sc_threshold','1000000000','-flags','+cgop','-pix_fmt','yuv420p','-color_range','tv','-colorspace','smpte170m','-f','mpeg1video']


def split_groups(data):
 starts=[];pos=0
 while True:
  pos=data.find(b'\x00\x00\x01',pos)
  if pos<0:break
  if pos+4>len(data):break
  starts.append((pos,data[pos+3]));pos+=4
 seq=None;pending=None;groups=[];pictures=0
 for j,(pos,code) in enumerate(starts):
  end=starts[j+1][0] if j+1<len(starts) else len(data)
  if code==0xb3:
   seq=data[pos:end]
   if len(seq)<8 or ((seq[4]<<4)|(seq[5]>>4))!=WIDTH or (((seq[5]&15)<<8)|seq[6])!=HEIGHT:raise ValueError('Unexpected MPEG dimensions')
   if (seq[7]&15)!=3:raise ValueError('MPEG frame rate must be 25 Hz')
   if pending is None:pending=pos
  elif code==0xb8:
   if pending is None:pending=pos
  elif code==0:
   if pos+6>len(data):raise ValueError('Truncated MPEG picture')
   picture_type=(data[pos+5]>>3)&7
   if picture_type not in (1,2):raise ValueError('B pictures are not permitted')
   if picture_type==1:
    if seq is None:raise ValueError('Missing sequence header')
    groups.append({'first':pictures,'start':pending if pending is not None else pos,'sequence':seq})
   elif not groups:raise ValueError('Stream does not start with an I picture')
   pictures+=1;pending=None
 if not groups:raise ValueError('No MPEG pictures')
 for i,g in enumerate(groups):
  end=groups[i+1]['start'] if i+1<len(groups) else len(data)
  g['count']=(groups[i+1]['first'] if i+1<len(groups) else pictures)-g['first']
  chunk=data[g['start']:end]
  if not chunk.startswith(b'\0\0\1\xb3'):chunk=g['sequence']+chunk
  if not chunk.endswith(b'\0\0\1\xb7'):chunk+=b'\0\0\1\xb7'
  g['colour']=chunk
 return groups,pictures


def metric(source,decoded):
 a=np.frombuffer(source,np.uint8).reshape(HEIGHT,WIDTH,3);b=np.frombuffer(decoded,np.uint8).reshape(HEIGHT,WIDTH,3)
 # Active source viewport only: black bars must not inflate the score.
 a=a[13:226];b=b[13:226];mae=float(np.abs(a.astype(np.int16)-b.astype(np.int16)).mean())
 x=cv2.cvtColor(a,cv2.COLOR_RGB2GRAY).astype(np.float32);y=cv2.cvtColor(b,cv2.COLOR_RGB2GRAY).astype(np.float32)
 mx=cv2.GaussianBlur(x,(11,11),1.5);my=cv2.GaussianBlur(y,(11,11),1.5)
 vx=cv2.GaussianBlur(x*x,(11,11),1.5)-mx*mx;vy=cv2.GaussianBlur(y*y,(11,11),1.5)-my*my
 cov=cv2.GaussianBlur(x*y,(11,11),1.5)-mx*my
 score=((2*mx*my+6.5025)*(2*cov+58.5225))/((mx*mx+my*my+6.5025)*(vx+vy+58.5225))
 return {'luma_ssim':float(score.mean()),'rgb_mae':mae}


def acceptable(metrics,cfg):return all(m['luma_ssim']>=cfg['min_luma_ssim'] and m['rgb_mae']<=cfg['max_rgb_mae'] for m in metrics)


def decode_bytes(data,ffmpeg):
 p=subprocess.run([str(ffmpeg),'-v','error','-nostdin','-f','mpegvideo','-i','pipe:0','-an','-fps_mode','passthrough','-f','rawvideo','-pix_fmt','rgb24','pipe:1'],input=data,stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True)
 return p.stdout


def depth_packet(raw,previous):
 from compress import predict,restore
 if not any(raw):return 0,b''
 if previous is not None and raw==previous:return 1,b''
 options=[]
 for temporal in [False,True] if previous is not None else [False]:
  values=(np.frombuffer(raw,np.uint8)-np.frombuffer(previous,np.uint8)).tobytes() if temporal else raw
  for predictor in range(3):
   body=lz4.block.compress(predict(values,WIDTH,1,predictor),mode='high_compression',compression=9,store_size=False)
   options.append((len(body),2+predictor+(3 if temporal else 0),body))
 _,mode,body=min(options,key=lambda x:x[0])
 check=restore(lz4.block.decompress(body,uncompressed_size=PIXELS),WIDTH,1,(mode-2)%3)
 if mode>=5:check=(np.frombuffer(check,np.uint8)+np.frombuffer(previous,np.uint8)).tobytes()
 if check!=raw:raise ValueError('Disparity lossless reconstruction failed')
 return mode,body


def encode(src,stereo_path,dest,ffmpeg,cfg,cuts,depth_source=None):
 from video import read_exact
 if depth_source is None:
  from stereo_resample import StereoSource
 dest=Path(dest);work=Path(tempfile.mkdtemp(prefix='mpeg-work-',dir=dest.parent));processes=[];logs=[]
 stereo=depth_source if depth_source is not None else StereoSource(stereo_path);offsets=[];records=[];mode_counts={};fallback_frames=0;colour_bytes=0;depth_bytes=0
 try:
  stream=work/'colour.m1v';force='expr:'+ '+'.join('eq(n,%d)'%n for n in sorted(set(cuts)|{0}))
  subprocess.run([str(ffmpeg),'-v','error','-nostdin','-y','-i',str(src),'-vf',FILTER+',format=yuv420p','-fps_mode','passthrough',*colour_args(cfg,cfg['quantizer']),'-force_key_frames',force,str(stream)],check=True)
  groups,total=split_groups(stream.read_bytes())
  if depth_source is not None and total!=stereo.n:raise ValueError('Original video/prepared map frame count mismatch')
  starts={g['first'] for g in groups}
  if any(c<total and c not in starts for c in cuts):raise ValueError('An authored cut did not receive an independent I picture')
  if any(g['count']>cfg['gop_frames'] for g in groups):raise ValueError('Unbounded MPEG seek group')
  # Verify each self-contained group with the local reference decoder while
  # colour is still on the workstation. No game-side video tests are run here.
  log=(work/'source.log').open('wb');logs.append(log)
  source=subprocess.Popen([str(ffmpeg),'-v','error','-nostdin','-i',str(src),'-vf',FILTER,'-an','-fps_mode','passthrough','-f','rawvideo','-pix_fmt','rgb24','pipe:1'],stdout=subprocess.PIPE,stderr=log);processes.append(source)
  spool=work/'payload'
  with spool.open('wb') as out:
   for g in groups:
    first,count=g['first'],g['count'];originals=[read_exact(source.stdout,PIXELS*3) for _ in range(count)]
    if any(len(x)!=PIXELS*3 for x in originals):raise ValueError('Source frame count does not match colour pictures')
    colour=g['colour'];decoded=decode_bytes(colour,ffmpeg)
    if len(decoded)!=count*PIXELS*3:raise ValueError('MPEG group frame count mismatch')
    scores=[metric(a,decoded[i*PIXELS*3:(i+1)*PIXELS*3]) for i,a in enumerate(originals)];initial=scores
    quantizer=cfg['quantizer']
    attempts=[]
    def rank(values):
     return (sum(m['luma_ssim']<cfg['min_luma_ssim'] or m['rgb_mae']>cfg['max_rgb_mae'] for m in values),-min(m['luma_ssim'] for m in values),max(m['rgb_mae'] for m in values))
    for retry_q in range(quantizer-1,cfg['retry_quantizer']-1,-1):
     if acceptable(scores,cfg):break
     p=subprocess.run([str(ffmpeg),'-v','error','-f','rawvideo','-pix_fmt','rgb24','-s','320x240','-r','25','-i','pipe:0',*colour_args(cfg,retry_q),'-frames:v',str(count),'pipe:1'],input=b''.join(originals),stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True)
     candidate=p.stdout
     if not candidate.endswith(b'\0\0\1\xb7'):candidate+=b'\0\0\1\xb7'
     retry_groups,retry_count=split_groups(candidate)
     if retry_count!=count or len(retry_groups)!=1:raise ValueError('Retry was not one independent MPEG group')
     decoded=decode_bytes(candidate,ffmpeg)
     if len(decoded)!=count*PIXELS*3:raise ValueError('Retry frame count mismatch')
     candidate_scores=[metric(a,decoded[i*PIXELS*3:(i+1)*PIXELS*3]) for i,a in enumerate(originals)]
     attempts.append({'quantizer':retry_q,'bytes':len(candidate),'min_luma_ssim':min(m['luma_ssim'] for m in candidate_scores),'max_rgb_mae':max(m['rgb_mae'] for m in candidate_scores)})
     if len(candidate)<=MAX_COLOUR and (acceptable(candidate_scores,cfg) or rank(candidate_scores)<rank(scores)):
      colour,scores,quantizer=candidate,candidate_scores,retry_q
    quality_failed=not acceptable(scores,cfg)
    # Colour subsampling imposes its own floor. A missed numerical target is
    # reported rather than silently expanding a short group into megabytes.
    lossless=(quality_failed and cfg['preserve_failed_groups_losslessly']) or len(colour)>MAX_COLOUR
    previous=None
    for local,original in enumerate(originals):
     frame=first+local;pos=frame*stereo.fps/FPS;low=int(pos);fraction=pos-low
     flow=stereo.flow(low) if fraction==0 else stereo.flow(low)*(1-fraction)+stereo.flow(low+1)*fraction
     raw=np.rint(flow*4).clip(-128,127).astype(np.int8).tobytes()
     if lossless:previous=None
     mode,depth=depth_packet(raw,previous);previous=raw;mode_counts[str(mode)]=mode_counts.get(str(mode),0)+1
     payload=lz4.block.compress(original,mode='high_compression',compression=9,store_size=False) if lossless else colour if local==0 else b''
     group_first=frame if lossless else first;group_count=1 if lossless else count
     colour_bytes+=len(payload);depth_bytes+=len(depth)
     offsets.append(out.tell());out.write(PACKET.pack(group_first,group_count,mode|(256 if lossless else 0),len(payload),len(depth)));out.write(payload);out.write(depth)
     fallback_frames+=lossless
    records.append({'first_frame':first,'frames':count,'quantizer':quantizer,'colour':'lossless RGB24/LZ4' if lossless else 'MPEG-1 I/P','delivered_min_luma_ssim':1.0 if lossless else min(m['luma_ssim'] for m in scores),'delivered_max_rgb_mae':0.0 if lossless else max(m['rgb_mae'] for m in scores),'initial_min_luma_ssim':min(m['luma_ssim'] for m in initial),'selected_min_luma_ssim':min(m['luma_ssim'] for m in scores),'selected_max_rgb_mae':max(m['rgb_mae'] for m in scores),'reference':'FFmpeg reconstruction; exact RGB source used on fallback','quality_target_met':not quality_failed,'retry_attempts':attempts,'fallback_reason':('quality threshold' if not acceptable(scores,cfg) else 'bounded group storage') if lossless else None})
    print('MPEG',Path(src).stem,first+count,'/',total,'RGB fallback' if lossless else 'q='+str(quantizer), 'kalite hedefi altinda' if quality_failed else '',flush=True)
   if read_exact(source.stdout,PIXELS*3) or source.wait():raise ValueError('Unexpected extra or missing source frames')
   end=out.tell()
  header=20+4*(total+1);temporary=dest.with_suffix(dest.suffix+'.part')
  with temporary.open('wb') as out,spool.open('rb') as inp:
   out.write(struct.pack('<4sHHIII',MAGIC,WIDTH,HEIGHT,FPS,total,cfg['gop_frames']));out.write(struct.pack('<%dI'%(total+1),*[header+x for x in offsets+[end]]));shutil.copyfileobj(inp,out)
  os.replace(temporary,dest)
  report={'format':'MPEG-1 colour / exact full-resolution quarter-pixel disparity','frames':total,'fps':FPS,'width':WIDTH,'height':HEIGHT,'bytes':dest.stat().st_size,'settings':cfg,'authored_cuts':[c for c in cuts if c<total],'lossless_colour_frames':fallback_frames,'lossless_colour_fraction':fallback_frames/total,'groups_below_target':sum(not g['quality_target_met'] for g in records),'colour_payload_bytes':colour_bytes,'disparity_payload_bytes':depth_bytes,'disparity_uncompressed_bytes':total*PIXELS,'colour_reference':'Source resampled with Lanczos to the existing 320x213 active viewport; black padding excluded from metrics','quality_decoder':'FFmpeg reference, not the ARM decoder; inspect on console after building','depth_modes':mode_counts,'groups':records,'hardware_tested':False,'arm_build_tested':False}
  write_json(dest.with_name('quality.json'),report);return total
 finally:
  for process in processes:
   if process.poll() is None:process.kill();process.wait()
   if process.stdout:process.stdout.close()
  for log in logs:log.close()
  stereo.f.close();shutil.rmtree(work,ignore_errors=True)


def header(path):
 with Path(path).open('rb') as f:
  magic,w,h,fps,n,group=struct.unpack('<4sHHIII',f.read(20))
  if magic!=MAGIC or (w,h,fps)!=(WIDTH,HEIGHT,FPS) or not 1<=n<=4000 or not 1<=group<=32:raise ValueError('Invalid MPEG container header')
  ix=struct.unpack('<%dI'%(n+1),f.read(4*(n+1)))
 if ix[0]!=20+4*(n+1) or ix[-1]!=Path(path).stat().st_size or any(b-a<PACKET.size or b-a>1024*1024 for a,b in zip(ix,ix[1:])):raise ValueError('Invalid MPEG container index')
 return n,group,ix


def packet(f,ix,index):
 f.seek(ix[index]);data=f.read(ix[index+1]-ix[index]);first,count,flags,colour,depth=PACKET.unpack_from(data)
 if flags&~0x107 or colour>MAX_COLOUR or depth>80000 or first+count>len(ix)-1 or colour+depth+PACKET.size!=len(data):raise ValueError('MPEG packet bounds')
 if count<1 or first>index or index>=first+count or (flags>>8 and (count!=1 or first!=index)) or (first!=index and colour):raise ValueError('MPEG group registration')
 return first,count,flags,data[PACKET.size:PACKET.size+colour],data[PACKET.size+colour:]


def decode_depth(mode,data,previous):
 from compress import restore
 if mode==0:
  if data:raise ValueError('Unexpected zero-map payload')
  return bytes(PIXELS)
 if mode==1:
  if data or previous is None:raise ValueError('Repeated map without reference')
  return previous
 raw=lz4.block.decompress(data,uncompressed_size=PIXELS);raw=restore(raw,WIDTH,1,(mode-2)%3)
 if mode>=5:
  if previous is None:raise ValueError('Predicted map without reference')
  raw=(np.frombuffer(raw,np.uint8)+np.frombuffer(previous,np.uint8)).tobytes()
 return raw


def read_frame(path,index,ffmpeg):
 n,limit,ix=header(path)
 if not 0<=index<n:raise ValueError('MPEG frame out of bounds')
 with Path(path).open('rb') as f:
  first,count,flags,colour,body=packet(f,ix,index)
  group_first,group_count,group_flags,encoded,_=packet(f,ix,first)
  if count>limit or group_first!=first or group_count!=count or not encoded:raise ValueError('Invalid independent colour group')
  if group_flags>>8:raw=lz4.block.decompress(encoded,uncompressed_size=PIXELS*3)
  else:
   decoded=decode_bytes(encoded,ffmpeg)
   if len(decoded)!=count*PIXELS*3:raise ValueError('Preview MPEG frame count')
   raw=decoded[(index-first)*PIXELS*3:(index-first+1)*PIXELS*3]
  disparity=None
  for frame in range(first,index+1):
   a,b,tag,_,body=packet(f,ix,frame)
   if (a,b,tag>>8)!=(first,count,group_flags>>8):raise ValueError('Cross-group dependency')
   disparity=decode_depth(tag&7,body,disparity)
 left=np.frombuffer(raw,np.uint8).reshape(HEIGHT,WIDTH,3).copy();left[:13]=0;left[226:]=0;disp=np.frombuffer(disparity,np.int8).reshape(HEIGHT,WIDTH).copy()
 yy,xx=np.mgrid[:HEIGHT,:WIDTH];q=(xx*4+disp).clip(0,1276);sx=q>>2;frac=(q&3)[:,:,None]
 right=((left[yy,sx].astype(np.uint16)*(4-frac)+left[yy,np.minimum(sx+1,319)].astype(np.uint16)*frac+2)>>2).astype(np.uint8)
 return Image.fromarray(left),Image.fromarray(right),disp


def resolve_ffmpeg(path=None,explicit=None):
 if explicit:return Path(explicit)
 configured=os.environ.get('LUME_FFMPEG')
 if configured and Path(configured).is_file():return Path(configured)
 found=shutil.which('ffmpeg')
 if found:return Path(found)
 if path is not None:
  for parent in Path(path).resolve().parents:
   folder=parent/'tools/detail/ffmpeg'
   if folder.is_dir():
    hits=list(folder.rglob('ffmpeg.exe'))
    if hits:return hits[0]
 raise ValueError('MPEG previews require the portable FFmpeg tool. Run EXE production first.')


def validate_file(path):
 n,limit,ix=header(path);previous=None;active_first=None;active_count=0;active_codec=None
 with Path(path).open('rb') as f:
  for frame in range(n):
   first,count,flags,colour,depth=packet(f,ix,frame)
   if first==frame:
    if active_first is not None and frame!=active_first+active_count:raise ValueError('MPEG group overlap or gap')
    if count>limit or first+count>n or not colour:raise ValueError('MPEG group bounds')
    active_first,active_count,active_codec=first,count,flags>>8;previous=None
    if flags>>8:
     if len(lz4.block.decompress(colour,uncompressed_size=PIXELS*3))!=PIXELS*3:raise ValueError('Lossless RGB payload')
    else:
     groups,decoded_count=split_groups(colour)
     if len(groups)!=1 or decoded_count!=count:raise ValueError('MPEG picture/group registration')
   elif (first,count,flags>>8)!=(active_first,active_count,active_codec):raise ValueError('Wrong MPEG group reference')
   previous=decode_depth(flags&7,depth,previous)
  if active_first+active_count!=n:raise ValueError('Incomplete final group')
 return n
