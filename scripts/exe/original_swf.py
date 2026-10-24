"""Lossless SWF display list recovery. No executable or ActionScript is run."""
import struct,zlib,io
from pathlib import Path
from PIL import Image
from recover import Bits,u16,u32,string
from swf_encoding import tag,matrix

def tags(data,start=0):
 p=start
 while p+2<=len(data):
  h=u16(data,p);p+=2;t=h>>6;n=h&63
  if n==63:n=u32(data,p);p+=4
  raw=data[p:p+n];p+=n
  if len(raw)!=n:raise ValueError('truncated tag')
  yield t,raw
  if t==0:break

def placement(t,b):
 f=b[0];f2=b[1] if t==70 else 0;p=2 if t==70 else 1
 e={'depth':u16(b,p),'move':bool(f&1)};p+=2
 if t==70 and ((f2&8) or ((f2&16) and (f&2))):_,p=string(b,p)
 if f&2:e['character']=u16(b,p);p+=2
 if f&4:
  bits=Bits(b,p);e['matrix']=bits.matrix();p=bits.p//8
 if f&8:
  start=p;bits=Bits(b,p);bits.cxform();p=bits.p//8;e['cx']=b[start:p]
 if f&16:e['ratio']=u16(b,p);p+=2
 if f&32:e['name'],p=string(b,p)
 if f&64:e['clip_depth']=u16(b,p);p+=2
 if f2&1:
  start=p;n=b[p];p+=1
  for _ in range(n):
   kind=b[p];p+=1
   if kind in (0,1,2,3,6):p+={0:23,1:9,2:15,3:27,6:80}[kind]
   elif kind in (4,7):p+=20+5*b[p]
   elif kind==5:p+=15+4*b[p]*b[p+1]
   else:raise ValueError('unknown filter')
  e['filters']=b[start:p]
 if f2&2:e['blend']=b[p];p+=1
 if f2&4:e['cache']=b[p];p+=1
 return e

def place(e):
 f=2;f2=0;b=struct.pack('<HH',e['depth'],e['character'])
 if 'matrix'in e:f|=4;b+=matrix(e['matrix'])
 if 'cx'in e:f|=8;b+=e['cx']
 if 'ratio'in e:f|=16;b+=struct.pack('<H',e['ratio'])
 if 'name'in e:f|=32;b+=e['name'].encode()+b'\0'
 if 'clip_depth'in e:f|=64;b+=struct.pack('<H',e['clip_depth'])
 if 'filters'in e:f2|=1;b+=e['filters']
 if 'blend'in e:f2|=2;b+=bytes([e['blend']])
 if 'cache'in e:f2|=4;b+=bytes([e['cache']])
 return tag(70,bytes([f,f2])+b) if f2 else tag(26,bytes([f])+b)

class Original:
 def __init__(self,path):
  b=Path(path).read_bytes();assert b[:3]==b'FWS';bits=Bits(b,8);self.rect=bits.rect();p=bits.p//8;self.fps=u16(b,p)/256
  self.payloads={};self.timelines={};self.defs=[]
  def scan(data,start=0,cid=0):
   events=[];fr=1
   for t,x in tags(data,start):
    events.append((fr,t,x))
    if t==1:fr+=1
    if t==39:scan(x,4,u16(x))
   self.timelines[cid]=events
  scan(b,p+4)
  for t,x in tags(b,p+4):
   if t in (0,1,12,26,70,28,5,43,59,9):continue
   if t in (2,6,7,10,11,14,20,21,22,32,33,34,35,36,37,39,46,48,60,75,83,84,87,88,90,91):self.payloads[u16(x)]=(t,x)
   self.defs.append((t,x))
 def states(self,cid):
  current={};result={};frame=1
  for fr,t,x in self.timelines[cid]:
   if t in (26,70):
    e=placement(t,x);d=e['depth']
    if not e['move']:current[d]={}
    current.setdefault(d,{}).update(e)
   elif t in (28,5):current.pop(u16(x,0 if t==28 else 2),None)
   elif t==1:
    result[fr]={d:e.copy() for d,e in current.items() if 'character'in e};frame=fr
  return result
 def state(self,cid,frame):return self.states(cid).get(frame,{})
 def snapshot(self,cid,frame):
  return struct.pack('<HH',cid,1)+b''.join(place(e) for d,e in sorted(self.state(cid,frame).items()) if not e.get('name','').startswith('hotspot'))+tag(1,b'')+tag(0,b'')
 def definitions(self,overrides=None):
  overrides=overrides or {};out=[]
  for t,x in self.defs:
   cid=u16(x) if len(x)>=2 else -1
   if cid in overrides and t==39:x=self.snapshot(cid,overrides[cid])
   elif t==39:
    items=[]
    for tt,xx in tags(x,4):
     if tt in (12,59):continue
     if tt in (26,70) and placement(tt,xx).get('name','').startswith('hotspot'):continue
     items.append(tag(tt,xx))
    x=x[:4]+b''.join(items)
   elif t==6:
    data=x[2:]
    if data[:4]==b'\xff\xd9\xff\xd8':data=data[2:]
    im=Image.open(io.BytesIO(data)).convert('RGBA');r,g,b,a=im.split();raw=Image.merge('RGBA',(a,r,g,b)).tobytes()
    t=36;x=struct.pack('<HBHH',cid,5,*im.size)+zlib.compress(raw)
   out.append(tag(t,x))
  return b''.join(out)
