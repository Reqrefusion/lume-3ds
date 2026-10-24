"""Verified local SWF extraction and portable conversion dependencies."""
from pathlib import Path,PurePosixPath

import argparse,hashlib,importlib.util,json,os,re,shutil,struct,subprocess,sys,time,urllib.request,zipfile,zlib

HERE=Path(__file__).resolve().parent




EXE_SHA='ba7a947b9a501a1723508e388764113e27b85db7bc14f52b613473373cad0089'

SWF_SHA='b09230e556b7b101170c168e18d4e86c8b5219e4daeb0c34a94acd98b3ae9bd4'

FFDEC_SHA='35f4930eb7c380afe66f2117f90b006deac0631473ad7500bb39c78f68645ecd'

FFDEC_JAR_SHA='1587e60b2ec2c0e4f5721fd5f2aaaf2379f6d416569de99054b5c5d1e9ba841c'

W,H=800,600

def digest(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  while b:=f.read(1024*1024):h.update(b)
 return h.hexdigest()

def dump(p,d):
 p=Path(p);p.parent.mkdir(parents=True,exist_ok=True);tmp=p.with_suffix(p.suffix+'.tmp');tmp.write_text(json.dumps(d,indent=2)+'\n',encoding='utf-8');os.replace(tmp,p)

def request(url):
 return urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'Lume-local-detail-converter','Accept':'application/json'}),timeout=60)

def fetch_json(url):
 with request(url) as f:return json.load(f)

def download(url,path,expected):
 if not re.fullmatch('[0-9a-fA-F]{64}',expected):raise ValueError('Missing tool SHA256.')
 path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
 if path.is_file() and digest(path)==expected.lower():return path
 tmp=path.with_suffix(path.suffix+'.part')
 for attempt in range(3):
  try:
   print('Downloading:',path.name,flush=True)
   with request(url) as src,tmp.open('wb') as dst:shutil.copyfileobj(src,dst,1024*1024)
   if digest(tmp)!=expected.lower():raise ValueError('Downloaded tool SHA256 mismatch: '+path.name)
   os.replace(tmp,path);return path
  except Exception:
   tmp.unlink(missing_ok=True)
   if attempt==2:raise
   time.sleep(1)

def unzip(src,dest):
 dest=Path(dest);stage=dest.with_name(dest.name+'.extracting');shutil.rmtree(stage,ignore_errors=True);stage.mkdir(parents=True)
 with zipfile.ZipFile(src) as z:
  for entry in z.infolist():
   p=PurePosixPath(entry.filename.replace('\\','/'))
   if p.is_absolute() or '..' in p.parts or ':' in str(p) or ((entry.external_attr>>16)&0o170000)==0o120000:raise ValueError('Invalid tool ZIP path.')
   if not (dest/str(p)).resolve().is_relative_to(dest.resolve()):raise ValueError('Tool ZIP path escapes the destination.')
  z.extractall(stage)
 if dest.exists():shutil.rmtree(dest)
 os.replace(stage,dest)

def find_one(folder,name):
 paths=list(Path(folder).rglob(name))
 if len(paths)!=1:raise ValueError('Tool missing or ambiguous: '+name)
 return paths[0].resolve()

def tools(cache,args,need_ffmpeg=True):
 # Explicit command-line tools support an existing offline installation and
 # the Linux verification harness. Default Windows tools are portable.
 downloads=cache/'downloads'
 installed_java=shutil.which('java')
 installed_ffmpeg=shutil.which('ffmpeg')
 installed_jar=Path(os.environ.get('LOCALAPPDATA',''))/'Programs/FFDec/ffdec.jar'
 if not args.java and installed_java:args.java=installed_java
 if not args.ffmpeg and installed_ffmpeg:args.ffmpeg=installed_ffmpeg
 if not args.ffdec and installed_jar.is_file() and digest(installed_jar)==FFDEC_JAR_SHA:args.ffdec=str(installed_jar)
 if args.java:java=Path(args.java).resolve()
 else:
  folder=cache/'java';hits=list(folder.rglob('java.exe'))
  if not hits:
   meta=fetch_json('https://api.adoptium.net/v3/assets/latest/17/hotspot?architecture=x64&image_type=jre&os=windows')[0]['binary']['package']
   p=download(meta['link'],downloads/meta['name'],meta['checksum']);unzip(p,folder)
  java=find_one(folder,'java.exe')
 if args.ffdec:jar=Path(args.ffdec).resolve()
 else:
  folder=cache/'ffdec';jar=folder/'ffdec.jar'
  if not jar.is_file() or digest(jar)!=FFDEC_JAR_SHA:
   p=download('https://github.com/jindrapetrik/jpexs-decompiler/releases/download/version26.3.0/ffdec_26.3.0.zip',downloads/'ffdec_26.3.0.zip',FFDEC_SHA);unzip(p,folder)
  jar=find_one(folder,'ffdec.jar')
 if digest(jar)!=FFDEC_JAR_SHA:raise ValueError('FFDec JAR SHA256 mismatch; no alternative renderer was used.')
 if not need_ffmpeg:ffmpeg=None
 elif args.ffmpeg:ffmpeg=Path(args.ffmpeg).resolve()
 else:
  folder=cache/'ffmpeg';hits=list(folder.rglob('ffmpeg.exe'))
  if not hits:
   meta=fetch_json('https://api.github.com/repos/BtbN/FFmpeg-Builds/releases/tags/latest')
   asset=next(a for a in meta['assets'] if a['name']=='ffmpeg-n8.1-latest-win64-lgpl-shared-8.1.zip')
   sha=asset.get('digest','').removeprefix('sha256:')
   p=download(asset['browser_download_url'],downloads/asset['name'],sha);unzip(p,folder)
   dump(cache/'ffmpeg-download.json',{'url':asset['browser_download_url'],'sha256':sha})
  ffmpeg=find_one(folder,'ffmpeg.exe')
 from java_compile import ensure as compile_exporters
 compile_exporters(Path(__file__).resolve().parent.parent,java,jar,download)
 return java,jar,ffmpeg

def source_swf(root,cache,args):
 out=cache/'Lume.swf'
 if out.is_file() and digest(out)==SWF_SHA:return out
 choices=[Path(args.exe)] if args.exe else [root/'Lume.exe',root/'Lume.exe',root/'Lume.swf',root/'Lume.swf']
 source=next((p for p in choices if p.is_file()),None)
 if source is None:raise ValueError('Place your original Lume.exe in the project directory or pass its path to the production pipeline.')
 data=source.read_bytes()
 if hashlib.sha256(data).hexdigest()==SWF_SHA:swf=data
 else:
  if hashlib.sha256(data).hexdigest()!=EXE_SHA:raise ValueError('Lume.exe does not match the supported original file.')
  offset=2447560;head=data[offset:offset+8]
  if head[:3]!=b'CWS' or struct.unpack_from('<I',head,4)[0]!=20560255:raise ValueError('Original SWF header not found.')
  dec=zlib.decompressobj();body=dec.decompress(data[offset+8:],20560255)
  if not dec.eof or len(body)!=20560255-8:raise ValueError('Incomplete or corrupt SWF.')
  swf=b'FWS'+head[3:]+body
 if hashlib.sha256(swf).hexdigest()!=SWF_SHA:raise ValueError('SWF SHA256 mismatch.')
 out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(swf);return out

def read_lan(path,full=False,dimensions=(800,600)):
 data=Path(path).read_bytes()
 if len(data)<24:raise ValueError('Incomplete LAN file: '+str(path))
 magic,w,h,fps,n=struct.unpack_from('<4s4I',data)
 if (magic,w,h,fps)!=(b'LAN2',*dimensions,25) or not 1<=n<=5000:raise ValueError('LAN basligi: '+str(path))
 if len(data)<20+4*(n+1):raise ValueError('Incomplete LAN index.')
 ix=struct.unpack_from('<%uI'%(n+1),data,20)
 if ix[0]!=20+4*(n+1) or ix[-1]!=len(data):raise ValueError('LAN indeks siniri.')
 packets=[]
 for i,(a,b) in enumerate(zip(ix,ix[1:])):
  if b-a<12:raise ValueError('LAN paket siniri.')
  bg,x,y,tw,th=struct.unpack_from('<IhhHH',data,a)
  if x<0 or y<0 or x+tw>w or y+th>h:raise ValueError('LAN kirpma siniri.')
  if full or i in (0,n-1):
   payload=data[a+12:b];size=tw*th*4
   if size:
    z=zlib.decompressobj();raw=z.decompress(payload,size+1)
    if not z.eof or z.unused_data or z.unconsumed_tail or len(raw)!=size:raise ValueError('LAN zlib boyutu.')
   elif payload:raise ValueError('Bos LAN paketi verili.')
  packets.append(data[a:b])
 return packets

def native_valid(path):
 path=Path(path)
 if path.suffix=='.lan':read_lan(path,dimensions=(320,240) if path.name=='studio_intro.lan' else (800,600))
 elif path.suffix=='.lta':read_lan(path,dimensions=(320,240))
 elif path.suffix=='.layout':
  data=path.read_bytes()
  if len(data)!=76 or data[:4]!=b'LTT1':raise ValueError('Title layout header')
  for i in range(9):
   x,y,w,h=struct.unpack_from('<4H',data,4+i*8)
   if not w or not h or x+w>320 or y+h>240:raise ValueError('Title touch rectangle')
 elif path.suffix=='.txt':
  data=path.read_bytes()
  if not 1<=len(data)<=(8192 if path.stem.startswith('doc_') else 4096) or any(b not in (9,10,13) and not 32<=b<=126 for b in data):raise ValueError('Invalid dialogue text: '+path.name)
  # An empty extracted entry selects the existing graphical speech fallback.
  # The exporter logs its label; it is not replaced with invented dialogue.
 elif path.name=='art.bin' and path.parent.name=='native_menu':
  data=path.read_bytes();dims=[(64,64)]*7+[(200,28)]+[(216,80)]*3
  header=b'LNA1'+struct.pack('<I',11)+b''.join(struct.pack('<HH',*d) for d in dims)
  if len(data)!=344500 or data[:52]!=header:raise ValueError('Invalid original menu atlas header or size.')
  pixels=memoryview(data)[52:]
  if any(pixels[i]>pixels[i+3] or pixels[i+1]>pixels[i+3] or pixels[i+2]>pixels[i+3] for i in range(0,len(pixels),4)):raise ValueError('Invalid original menu premultiplied alpha.')
  at=0
  for w,h in dims:
   if not any(pixels[at+3:at+w*h*4:4]):raise ValueError('Empty original menu drawing.')
   at+=w*h*4
 elif path.name in ('paper320.bin','paper400.bin'):
  width=int(path.stem[5:]);data=path.read_bytes()
  if len(data)!=8+width*240*4 or data[:8]!=struct.pack('<4sHH',b'LRP2',width,240):raise ValueError('Invalid original speech texture: '+path.name)
 elif path.suffix=='.bin' and path.name=='font.bin':
  data=path.read_bytes()
  if len(data)!=22808 or data[:8]!=b'LRF1\x0c\x14\x5f\x00' or not any(data[8:]):raise ValueError('Invalid reading font.')
 elif path.suffix=='.rgb':
  if path.stat().st_size!=800*600*3:raise ValueError('RGB boyutu: '+str(path))
 elif path.suffix=='.rgba':
  if path.stat().st_size!=800*600*4:raise ValueError('RGBA boyutu: '+str(path))
 else:raise ValueError('Unexpected production file: '+str(path))

def run_logged(cmd,cwd):
 process=subprocess.Popen(cmd,cwd=cwd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding='utf-8',errors='replace')
 for line in process.stdout:print(line.rstrip(),flush=True)
 if process.wait():raise subprocess.CalledProcessError(process.returncode,cmd)

