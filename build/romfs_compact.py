"""Share byte-identical payloads in raw 3DSX RomFS; preserve all names and code.
Format matches libctru's romfs_header/romfs_dir/romfs_file (not NCCH/IVFC).
"""
from pathlib import Path
import hashlib,os,struct,tempfile

def read_index(image):
 image=Path(image);length=image.stat().st_size
 with image.open('rb') as f:
  header=f.read(44)
  if len(header)!=44 or header[:4]!=b'3DSX':raise ValueError('Invalid extended 3DSX header')
  base=struct.unpack_from('<I',header,40)[0]
  if base<44 or base+40>length:raise ValueError('Missing raw RomFS')
  f.seek(base);h=struct.unpack('<10I',f.read(40))
  if h[0]!=40 or base+h[9]>length:raise ValueError('Invalid RomFS header')
  for off,size in [(h[1],h[2]),(h[3],h[4]),(h[5],h[6]),(h[7],h[8])]:
   if off<40 or off+size>h[9]:raise ValueError('Invalid metadata range')
  f.seek(base);meta=bytearray(f.read(h[9]));dirs={};pos=h[3];end=pos+h[4]
  while pos<end:
   if pos+24>end:raise ValueError('Truncated directory')
   parent,_,_,_,_,n=struct.unpack_from('<6I',meta,pos)
   if n%2 or pos+24+n>end:raise ValueError('Invalid directory name')
   name=bytes(meta[pos+24:pos+24+n]).decode('utf-16le')
   if '/' in name or '\\' in name or name in ['.','..'] or '\0' in name:raise ValueError('Unsafe directory name')
   dirs[pos-h[3]]=(parent,name);pos+=(24+n+3)&~3
  def dirname(offset):
   parts=[];seen=set()
   while True:
    if offset in seen or offset not in dirs:raise ValueError('Invalid directory ancestry')
    seen.add(offset);parent,name=dirs[offset]
    if not name:
     if offset!=0:raise ValueError('Invalid root directory')
     return '/'.join(reversed(parts))
    parts.append(name);offset=parent
  files=[];pos=h[7];end=pos+h[8]
  while pos<end:
   if pos+32>end:raise ValueError('Truncated file')
   parent,_,offset,size,_,n=struct.unpack_from('<IIQQII',meta,pos)
   if n%2 or pos+32+n>end or base+h[9]+offset+size>length:raise ValueError('Invalid file range')
   name=bytes(meta[pos+32:pos+32+n]).decode('utf-16le')
   if not name or '/' in name or '\\' in name or name in ['.','..'] or '\0' in name:raise ValueError('Unsafe file name')
   folder=dirname(parent);files.append((folder+'/'+name if folder else name,offset,size,pos));pos+=(32+n+3)&~3
  if len({x[0] for x in files})!=len(files):raise ValueError('Duplicate file name')
  return base,h[9],meta,files

def inventory(image):
 base,start,_,files=read_index(image);result={}
 with Path(image).open('rb') as f:
  for name,offset,size,_ in files:
   f.seek(base+start+offset);data=f.read(size)
   result[name]={'bytes':size,'sha256':hashlib.sha256(data).hexdigest()}
 return result

def compact_3dsx(source,dest,expected=None):
 source,dest=Path(source),Path(dest)
 if source.resolve()==dest.resolve():raise ValueError('Source and destination must differ')
 base,start,meta,files=read_index(source);unique={};regions=[];payload=0;shared=0
 with source.open('rb') as f:
  for name,offset,size,position in files:
   f.seek(base+start+offset);data=f.read(size);digest=hashlib.sha256(data).hexdigest();key=(size,digest)
   if expected is not None and expected.get(name)!={'bytes':size,'sha256':digest}:raise ValueError('Source asset differs: '+name)
   if key in unique:
    target,old=unique[key];f.seek(base+start+old)
    if f.read(size)!=data:raise ValueError('Digest collision')
    shared+=1
   else:
    payload=(payload+15)&~15;target=payload;unique[key]=(target,offset);regions.append((target,offset,size));payload+=size
   struct.pack_into('<Q',meta,position+8,target)
  if expected is not None and set(expected)!={x[0] for x in files}:raise ValueError('Asset names differ')
  dest.parent.mkdir(parents=True,exist_ok=True)
  fd,tmp=tempfile.mkstemp(prefix=dest.name+'.',suffix='.part',dir=dest.parent);os.close(fd);tmp=Path(tmp)
  try:
   with tmp.open('wb') as out:
    f.seek(0);prefix=f.read(base);out.write(prefix);out.write(meta)
    for target,offset,size in regions:
     gap=base+start+target-out.tell()
     if gap<0:raise ValueError('Overlapping output')
     out.write(b'\0'*gap);f.seek(base+start+offset);out.write(f.read(size))
   if inventory(tmp)!=(expected if expected is not None else inventory(source)):raise ValueError('Compacted asset verification failed')
   with tmp.open('rb') as check:
    if check.read(base)!=prefix:raise ValueError('Executable prefix differs')
   os.replace(tmp,dest)
  finally:tmp.unlink(missing_ok=True)
 return {'original_bytes':source.stat().st_size,'bytes':dest.stat().st_size,'saved_bytes':source.stat().st_size-dest.stat().st_size,'files':len(files),'unique_payloads':len(unique),'shared_entries':shared,'verified':True}
