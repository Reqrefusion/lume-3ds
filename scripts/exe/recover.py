#!/usr/bin/env python3
"""Static SWF/AVM1 recovery. Never executes the input executable or scripts."""
import argparse, collections, hashlib, io, json, struct, zlib
from pathlib import Path
from PIL import Image

def u16(b, p=0): return struct.unpack_from('<H', b, p)[0]
def u32(b, p=0): return struct.unpack_from('<I', b, p)[0]
def string(b, p=0):
    e=b.index(0,p)
    return b[p:e].decode('utf-8','replace'),e+1

def save_png(image,path):
    # Encode in memory, then write atomically as one complete byte stream.
    stream=io.BytesIO();image.save(stream,format='PNG');path.write_bytes(stream.getvalue())

class Bits:
    def __init__(self,b,p=0): self.b=b; self.p=p*8
    def get(self,n,signed=False):
        v=0
        for _ in range(n):
            v=(v<<1)|((self.b[self.p//8]>>(7-self.p%8))&1); self.p+=1
        return v-(1<<n) if signed and n and v&(1<<(n-1)) else v
    def align(self): self.p=(self.p+7)//8*8
    def rect(self):
        n=self.get(5); out=[self.get(n,True) for _ in range(4)];self.align();return out
    def matrix(self):
        sx=sy=1.;r0=r1=0.
        if self.get(1):
            n=self.get(5);sx=self.get(n,True)/65536;sy=self.get(n,True)/65536
        if self.get(1):
            n=self.get(5);r0=self.get(n,True)/65536;r1=self.get(n,True)/65536
        n=self.get(5);tx=self.get(n,True)/20;ty=self.get(n,True)/20;self.align()
        return [sx,r0,r1,sy,tx,ty]
    def cxform(self,alpha=True):
        add=self.get(1);mul=self.get(1);n=self.get(4);c=4 if alpha else 3
        if mul:
            for _ in range(c):self.get(n,True)
        if add:
            for _ in range(c):self.get(n,True)
        self.align()

OPS={0:'End',4:'NextFrame',5:'PreviousFrame',6:'Play',7:'Stop',8:'ToggleQuality',9:'StopSounds',
10:'Add',11:'Subtract',12:'Multiply',13:'Divide',14:'Equals',15:'Less',16:'And',17:'Or',18:'Not',
19:'StringEquals',20:'StringLength',21:'StringExtract',23:'Pop',24:'ToInteger',28:'GetVariable',29:'SetVariable',
32:'SetTarget2',33:'StringAdd',34:'GetProperty',35:'SetProperty',36:'CloneSprite',37:'RemoveSprite',38:'Trace',
39:'StartDrag',40:'EndDrag',41:'StringLess',42:'Throw',43:'CastOp',44:'ImplementsOp',48:'RandomNumber',
49:'MBStringLength',50:'CharToAscii',51:'AsciiToChar',52:'GetTime',53:'MBStringExtract',54:'MBCharToAscii',55:'MBAsciiToChar',
58:'Delete',59:'Delete2',60:'DefineLocal',61:'CallFunction',62:'Return',63:'Modulo',64:'NewObject',65:'DefineLocal2',
66:'InitArray',67:'InitObject',68:'TypeOf',69:'TargetPath',70:'Enumerate',71:'Add2',72:'Less2',73:'Equals2',
74:'ToNumber',75:'ToString',76:'PushDuplicate',77:'StackSwap',78:'GetMember',79:'SetMember',80:'Increment',
81:'Decrement',82:'CallMethod',83:'NewMethod',84:'InstanceOf',85:'Enumerate2',96:'BitAnd',97:'BitOr',98:'BitXor',
99:'BitLShift',100:'BitRShift',101:'BitURShift',102:'StrictEquals',103:'Greater',104:'StringGreater',105:'Extends',
129:'GotoFrame',131:'GetURL',135:'StoreRegister',136:'ConstantPool',138:'WaitForFrame',139:'SetTarget',140:'GotoLabel',
141:'WaitForFrame2',142:'DefineFunction2',143:'Try',148:'With',150:'Push',153:'Jump',154:'GetURL2',155:'DefineFunction',
157:'If',158:'Call',159:'GotoFrame2'}

def actions(b,pool=None):
    pool=list(pool or []); out=[];p=0
    while p<len(b):
        start=p;op=b[p];p+=1;n=0
        if op>=128:n=u16(b,p);p+=2
        x=b[p:p+n];p+=n
        if len(x)!=n:raise ValueError('Truncated action')
        a={'offset':start,'opcode':op,'op':OPS.get(op,f'Unknown_{op:02x}')}
        if op==136:
            q=2;pool=[]
            for _ in range(u16(x)):s,q=string(x,q);pool.append(s)
            a['constants']=pool
        elif op==150:
            q=0;v=[]
            while q<len(x):
                t=x[q];q+=1
                if t==0:s,q=string(x,q);v.append(s)
                elif t==1:v.append(struct.unpack_from('<f',x,q)[0]);q+=4
                elif t==2:v.append(None)
                elif t==3:v.append({'undefined':True})
                elif t==4:v.append({'register':x[q]});q+=1
                elif t==5:v.append(bool(x[q]));q+=1
                elif t==6:v.append(struct.unpack('<d',x[q+4:q+8]+x[q:q+4])[0]);q+=8
                elif t==7:v.append(struct.unpack_from('<i',x,q)[0]);q+=4
                elif t in (8,9):
                    i=x[q] if t==8 else u16(x,q);q+=1 if t==8 else 2
                    v.append(pool[i] if i<len(pool) else {'unresolved_constant':i})
                else:raise ValueError(f'Unknown push type {t}')
            a['values']=v
        elif op in (142,155):
            name,q=string(x);argc=u16(x,q);q+=2;params=[];flags=0
            if op==142:a['register_count']=x[q];flags=u16(x,q+1);q+=3
            for _ in range(argc):
                reg=x[q] if op==142 else 0;q+=1 if op==142 else 0
                s,q=string(x,q);params.append({'name':s,'register':reg})
            size=u16(x,q)
            a.update(name=name,params=params,flags=flags,body=actions(b[p:p+size],pool));p+=size
        elif op in (153,157):a['target']=p+struct.unpack('<h',x)[0]
        elif op in (139,140):a['value']=string(x)[0]
        elif op==131:
            url,q=string(x);target,_=string(x,q);a.update(url=url,target=target)
        elif op==129:a['frame']=u16(x)+1
        elif op==135:a['register']=x[0]
        elif op==148:
            size=u16(x);a['body']=actions(b[p:p+size],pool);p+=size
        elif x:a['raw']=x.hex()
        a['next_offset']=p;out.append(a)
        if op==0:break
    return out

def listing(code,indent=''):
    lines=[]
    for a in code:
        fields={k:v for k,v in a.items() if k not in ('offset','next_offset','opcode','op','body')}
        lines.append(f"{indent}{a['offset']:05x} {a['op']} {json.dumps(fields,ensure_ascii=False)}")
        if 'body' in a:lines.extend(listing(a['body'],indent+'    '))
    return lines

class Recovery:
    def __init__(self,dest):
        self.dest=dest;self.tags=collections.Counter();self.scripts=[];self.symbols={};self.timelines={};self.assets=[];self.videos={};self.errors=[]
        for name in ('scripts','images','audio','video','raw'): (dest/name).mkdir(parents=True,exist_ok=True)
    def save_script(self,b,context):
        i=len(self.scripts);code=actions(b);name=f'{i:04d}'
        (self.dest/'scripts'/f'{name}.json').write_text(json.dumps({'context':context,'actions':code},ensure_ascii=False,indent=2), encoding="utf-8")
        (self.dest/'scripts'/f'{name}.txt').write_text(context+'\n'+'\n'.join(listing(code)), encoding="utf-8")
        self.scripts.append({'id':i,'context':context,'bytes':len(b)})
    def scan(self,b,p=0,owner=0):
        frame=1;timeline=[]
        while p+2<=len(b):
            start=p;h=u16(b,p);p+=2;t=h>>6;n=h&63
            if n==63:n=u32(b,p);p+=4
            x=b[p:p+n];p+=n
            if len(x)!=n:raise ValueError('Truncated tag')
            self.tags[t]+=1
            event={'tag':t,'frame':frame,'length':n}
            try:
                if t==39:
                    ident=u16(x);event.update(character=ident,frames=u16(x,2));self.scan(x,4,ident)
                elif t in (12,59):
                    context=f'timeline={owner}, frame={frame}'
                    if t==59:context+=f', init={u16(x)}'
                    self.save_script(x[2:] if t==59 else x,context)
                elif t in (56,76):
                    q=2
                    for _ in range(u16(x)):
                        ident=u16(x,q);s,q=string(x,q+2);self.symbols[ident]=s
                elif t==43:event['label']=string(x)[0]
                elif t in (6,21,35,90):self.jpeg(t,x)
                elif t in (20,36):self.lossless(t,x)
                elif t==14:self.sound(x)
                elif t==60:
                    ident=u16(x);self.videos[ident]={'frames':u16(x,2),'width':u16(x,4),'height':u16(x,6),'codec':x[9],'packets':[]}
                elif t==61:self.videos[u16(x)]['packets'].append((u16(x,2),x[4:]))
                elif t in (26,70):
                    flags=x[0];q=1
                    if t==70:q+=1
                    event.update(flags=flags,depth=u16(x,q));q+=2
                    if t==70 and x[1]&8 and (x[1]&16 or flags&2):_,q=string(x,q)
                    if flags&2:event['character']=u16(x,q);q+=2
                    if flags&4:
                        bit=Bits(x,q);event['matrix']=bit.matrix();q=bit.p//8
                    if flags&8:
                        bit=Bits(x,q);bit.cxform();q=bit.p//8
                    if flags&16:event['ratio']=u16(x,q);q+=2
                    if flags&32:event['name'],q=string(x,q)
                    if flags&64:event['clip_depth']=u16(x,q);q+=2
                    if flags&128 and t==26:
                        q+=6
                        while q+4<=len(x):
                            events=u32(x,q);q+=4
                            if not events:break
                            size=u32(x,q);q+=4;payload=x[q:q+size];q+=size
                            if events&0x20000:payload=payload[1:]
                            self.save_script(payload,f'timeline={owner}, frame={frame}, depth={event["depth"]}, clip_events=0x{events:x}')
                elif t==34:
                    off=u16(x,3)
                    if off:
                        q=3+off
                        while q+4<=len(x):
                            size=u16(x,q);cond=u16(x,q+2);end=q+size if size else len(x)
                            self.save_script(x[q+4:end],f'button={u16(x)}, conditions=0x{cond:x}')
                            if not size:break
                            q=end
                if t in (2,22,32,83,46,84,34,37,11,33,48,75):
                    ident=u16(x);(self.dest/'raw'/f'{ident:04d}_tag{t}.bin').write_bytes(x)
            except Exception as e:self.errors.append({'timeline':owner,'frame':frame,'tag':t,'offset':start,'error':str(e)})
            timeline.append(event)
            if t==1:frame+=1
            if t==0:break
        self.timelines[owner]=timeline
    def jpeg(self,t,x):
        ident=u16(x);data=x[2:];alpha=None
        if t in (35,90):
            n=u32(x,2);q=8 if t==90 else 6;data=x[q:q+n];alpha=zlib.decompress(x[q+n:])
        # Flash may insert an erroneous EOI/SOI pair at the start.
        if data.startswith(b'\xff\xd9\xff\xd8'):data=data[2:]
        image=Image.open(io.BytesIO(data)).convert('RGBA')
        if alpha:image.putalpha(Image.frombytes('L',image.size,alpha))
        path=f'images/{ident:04d}.png';save_png(image,self.dest/path)
        self.assets.append({'id':ident,'kind':'image','path':path,'width':image.width,'height':image.height})
    def lossless(self,t,x):
        ident=u16(x);fmt=x[2];w=u16(x,3);h=u16(x,5);q=7
        if fmt!=5:
            self.assets.append({'id':ident,'kind':'lossless_raw','format':fmt});(self.dest/'raw'/f'{ident:04d}_tag{t}.bin').write_bytes(x);return
        data=zlib.decompress(x[q:]);im=Image.new('RGBA',(w,h));pixels=[]
        for a,r,g,b in zip(data[0::4],data[1::4],data[2::4],data[3::4]):
            if t==20:a=255
            if a and t==36:r=min(255,round(r*255/a));g=min(255,round(g*255/a));b=min(255,round(b*255/a))
            pixels.append((r,g,b,a))
        im.putdata(pixels);path=f'images/{ident:04d}.png';save_png(im,self.dest/path)
        self.assets.append({'id':ident,'kind':'image','path':path,'width':w,'height':h})
    def sound(self,x):
        ident=u16(x);fmt=x[2]>>4;rate=[5512,11025,22050,44100][(x[2]>>2)&3];samples=u32(x,3)
        path=f'audio/{ident:04d}.mp3' if fmt==2 else f'audio/{ident:04d}.bin'
        (self.dest/path).write_bytes(x[9:] if fmt==2 else x[7:])
        self.assets.append({'id':ident,'kind':'sound','format':fmt,'rate':rate,'samples':samples,'path':path})
    def finish(self,header):
        for ident,v in self.videos.items():
            output=bytearray(b'FLV\x01\x01\x00\x00\x00\x09\x00\x00\x00\x00')
            unique={}
            for frame,payload in v['packets']:
                if frame in unique and unique[frame]!=payload:
                    self.errors.append({'video':ident,'frame':frame,'error':'Conflicting duplicate video packet'})
                unique[frame]=payload
            v['recovered_frames']=len(unique)
            v['duplicate_packets']=len(v['packets'])-len(unique)
            for frame,payload in sorted(unique.items()):
                timestamp=round(frame*1000/header['fps'])
                # FLV VP6 adds a crop-adjustment byte absent from SWF VideoFrame.
                extra=bytes([((-v['width'])%16)*16+(-v['height'])%16]) if v['codec']==4 else b''
                keyframe=not (payload[0]&0x80) if v['codec']==4 else True
                packet=bytes([(0x10 if keyframe else 0x20)|v['codec']])+extra+payload;size=len(packet)
                output+=bytes([9])+size.to_bytes(3,'big')+(timestamp&0xffffff).to_bytes(3,'big')+bytes([timestamp>>24])+b'\x00\x00\x00'+packet+(11+size).to_bytes(4,'big')
            path=f'video/{ident:04d}.flv';(self.dest/path).write_bytes(output)
            self.assets.append({k:val for k,val in {'id':ident,'kind':'video','path':path,**v}.items() if k!='packets'})
        report={**header,'tag_counts':dict(self.tags),'symbols':self.symbols,'scripts':self.scripts,'assets':self.assets,'timelines':self.timelines,'errors':self.errors}
        (self.dest/'manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2), encoding="utf-8")
        return report

def main():
    ap=argparse.ArgumentParser();ap.add_argument('exe',type=Path);ap.add_argument('destination',type=Path);a=ap.parse_args();b=a.exe.read_bytes();candidates=[];pos=0
    while True:
        pos=b.find(b'CWS',pos)
        if pos<0:break
        try:
            size=u32(b,pos+4)
            if b[pos+3]<=40 and 8<size<100_000_000:
                dec=zlib.decompressobj();body=dec.decompress(b[pos+8:]);body+=dec.flush()
                if dec.eof and len(body)+8==size:candidates.append((size,pos,body))
        except (zlib.error,struct.error):pass
        pos+=3
    if not candidates:raise ValueError('No verified SWF found')
    size,pos,body=max(candidates);a.destination.mkdir(parents=True,exist_ok=True)
    swf=b'FWS'+b[pos+3:pos+8]+body;(a.destination/'Lume.swf').write_bytes(swf)
    bit=Bits(body);rect=bit.rect();p=bit.p//8;fps=u16(body,p)/256;frames=u16(body,p+2)
    header={'exe_sha256':hashlib.sha256(b).hexdigest(),'swf_sha256':hashlib.sha256(swf).hexdigest(),'exe_swf_offset':pos,'swf_version':b[pos+3],'swf_bytes':size,'stage_rect_twips':rect,'fps':fps,'frames':frames}
    rec=Recovery(a.destination);rec.scan(body,p+4);report=rec.finish(header)
    print(json.dumps({'stage':rect,'fps':fps,'scripts':len(rec.scripts),'assets':len(rec.assets),'errors':rec.errors},indent=2))

if __name__=='__main__':main()
