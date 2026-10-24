"""Export validated geometry only; original media and executables are forbidden."""
from pathlib import Path
import argparse,hashlib,json,struct,tempfile,zipfile
ROOT=Path(__file__).resolve().parents[1]
HEADER=struct.Struct('<4sHHIII');PACKET=struct.Struct('<IHHII');MAP=struct.Struct('<IHBI')
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def video(source,dest):
 packets=[];hashes=[]
 with source.open('rb') as f:
  magic,w,h,fps,n,group=HEADER.unpack(f.read(20))
  if magic!=b'LVM1' or (w,h,fps)!=(320,240,25) or not 1<=n<=4000:raise ValueError('Unsupported video')
  ix=struct.unpack('<%dI'%(n+1),f.read(4*(n+1)))
  if ix[0]!=20+4*(n+1) or ix[-1]!=source.stat().st_size:raise ValueError('Video index')
  for i in range(n):
   if not 16<=ix[i+1]-ix[i]<=1048576:raise ValueError('Packet bounds')
   f.seek(ix[i]);first,count,flags,colour,depth=PACKET.unpack(f.read(16))
   if flags&~0x107 or not first<=i<first+count or first+count>n or ix[i+1]-ix[i]!=16+colour+depth:raise ValueError('Source packet')
   f.seek(colour,1);body=f.read(depth)
   if len(body)!=depth:raise ValueError('Truncated geometry')
   packet=MAP.pack(first,count,flags&7,depth)+body;packets.append(packet);hashes.append(hashlib.sha256(packet).digest())
 offsets=[20+4*(n+1)]
 for packet in packets:offsets.append(offsets[-1]+len(packet))
 with dest.open('xb') as out:
  out.write(HEADER.pack(b'LDP1',w,h,fps,n,group));out.write(struct.pack('<%dI'%(n+1),*offsets))
  for packet in packets:out.write(packet)
 with dest.open('rb') as f:
  for i,expected in enumerate(hashes):
   f.seek(offsets[i])
   if hashlib.sha256(f.read(offsets[i+1]-offsets[i])).digest()!=expected:raise ValueError('Map export mismatch')
 return {'frames':n,'bytes':dest.stat().st_size,'colour_bytes_copied':0}
README='''# Lume 3DS geometry maps

Prepared disparity/depth maps only. No original video, music, audio, color image, game executable, SWF, font atlas or ActionScript is included.

SDF1: compressed still-image quarter-pixel correspondence. PDP1: puzzle displacement. TSF1: indexed title displacement. LDP1: indexed video displacement without its color payload.

LDP1 uses little-endian <4sHHIII> (magic, width, height, fps, frame count, group limit), frame_count+1 uint32 offsets, and packets <IHBI> (group first frame, group frame count, depth mode, depth byte count) followed by depth bytes. Prediction modes match the LVM1 disparity decoder. Original group registration is retained. No color bytes are stored.

These maps are not an installable game by themselves. Use SETUP.cmd from the source/player package to combine them with your own original Lume.exe. The PC player path reuses the geometry without depth inference or a 3DS compiler and copies the fixed application unchanged beside external content in SD:/3ds/lume/. Direct EXE conversion on a console is not implemented.
'''
def export(rom,dest):
 rom=Path(rom).resolve();dest=Path(dest).resolve()
 if dest.exists():raise ValueError('Use a new output directory')
 dest.mkdir(parents=True);records={};videos={}
 with tempfile.TemporaryDirectory(prefix='maps-only-') as temp:
  temp=Path(temp)
  for p in sorted(rom.rglob('*')):
   if not p.is_file() or p.suffix not in {'.sdf','.depth','.tsf'}:continue
   with p.open('rb') as f:header=f.read(16)
   if header[:4]!={'.sdf':b'SDF1','.depth':b'PDP1','.tsf':b'TSF1'}[p.suffix]:raise ValueError('Unexpected map format')
   if p.suffix in {'.sdf','.depth'}:
    w,h=struct.unpack_from('<HH',header,4)
    if not(1<=w<=320 and 1<=h<=240):raise ValueError('Map dimensions')
    if p.suffix=='.depth' and p.stat().st_size!=8+w*h:raise ValueError('Puzzle map size')
   else:
    w,h,n=struct.unpack_from('<III',header,4)
    if (w,h)!=(160,120) or not 1<=n<=4000:raise ValueError('Title map header')
   target=temp/'maps'/p.relative_to(rom);target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(p.read_bytes())
  for p in sorted(rom.glob('*.lvs')):
   target=temp/'maps/video'/(p.stem+'.ldp');target.parent.mkdir(parents=True,exist_ok=True);videos[p.stem]=video(p,target)
  for p in sorted((temp/'maps').rglob('*')):
   if p.is_file():records[p.relative_to(temp).as_posix()]={'bytes':p.stat().st_size,'sha256':sha(p)}
  if not records or len(videos)!=4:raise ValueError('Incomplete ready maps')
  report={'format':1,'content':'geometry-only','original_media_included':False,'executable_included':False,'original_exe_included':False,'map_generation_run':False,'installable_game':False,'files':records,'video_maps':videos}
  (temp/'maps-manifest.json').write_text(json.dumps(report,indent=2),encoding='utf-8');(temp/'README.md').write_text(README,encoding='utf-8')
  with zipfile.ZipFile(dest/'Lume3DS-Maps-Only.zip','w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
   for p in sorted(temp.rglob('*')):
    if p.is_file():z.write(p,p.relative_to(temp).as_posix())
 with zipfile.ZipFile(dest/'Lume3DS-Maps-Only.zip') as z:
  if z.testzip():raise ValueError('ZIP verification failed')
  for name in z.namelist():
   if name not in ['README.md','maps-manifest.json'] and (not name.startswith('maps/') or Path(name).suffix not in {'.sdf','.depth','.tsf','.ldp'}):raise ValueError('Non-geometry entry')
 (dest/'MAPS_AUDIT.json').write_text(json.dumps(report,indent=2),encoding='utf-8');(dest/'README.md').write_text(README,encoding='utf-8')
 (dest/'SHA256SUMS.txt').write_text(sha(dest/'Lume3DS-Maps-Only.zip')+'  Lume3DS-Maps-Only.zip\n',encoding='ascii')
 print('Geometry maps:',len(records),'ZIP bytes:',(dest/'Lume3DS-Maps-Only.zip').stat().st_size)
 print('Video frames verified:',sum(v['frames'] for v in videos.values()),'; original color/audio bytes copied: 0')
 return report
def prepared_files(root):
 root=Path(root).resolve();folder=root/'maps'
 manifest=json.loads((folder/'manifest.json').read_text(encoding='utf-8'))
 records=manifest['files'];allowed={'.sdf':b'SDF1','.depth':b'PDP1','.tsf':b'TSF1','.ldp':b'LDP1'}
 if not records or manifest.get('content')!='geometry-only':raise ValueError('Geometry manifest required')
 expected={'maps/README.md','maps/manifest.json'}
 for name,record in records.items():
  relative=Path(name)
  if relative.is_absolute() or '..' in relative.parts or '\\' in name or ':' in name or not name.startswith('maps/') or relative.suffix not in allowed:raise ValueError('Unsafe geometry path')
  path=root/relative
  if path.is_symlink() or not path.resolve().is_relative_to(folder.resolve()):raise ValueError('Geometry path escapes maps directory')
  if not path.is_file() or path.stat().st_size!=record['bytes'] or sha(path)!=record['sha256']:raise ValueError('Geometry checksum mismatch: '+name)
  with path.open('rb') as stream:
   if stream.read(4)!=allowed[relative.suffix]:raise ValueError('Geometry format mismatch: '+name)
  expected.add(name)
 actual={p.relative_to(root).as_posix() for p in folder.rglob('*') if p.is_file()}
 if actual!=expected:raise ValueError('Unexpected or missing file in maps directory')
 return sorted(expected)

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--romfs',default=str(ROOT/'romfs'));p.add_argument('--dest',required=True);a=p.parse_args();export(a.romfs,a.dest)