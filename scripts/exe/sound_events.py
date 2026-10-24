"""Offline SWF SoundInfo baking. Event-specific IDs keep SFX2 compatible.

The console only plays precomputed PCM; no per-frame envelope processing.
In/out and envelope positions use Flash's 44.1 kHz sample clock.
"""
from pathlib import Path
import json,re,struct,subprocess
BASE_ID=32768
MAX_EVENTS=256
MAX_BANK_BYTES=16*1024*1024

def supported_effects(source):
    text=(Path(source)/'game/lume/audio.c').read_text(encoding='utf-8-sig')
    block=text.split('static EffectSample samples[] = {',1)[1].split('};',1)[0]
    return {int(n) for n in re.findall(r'\.id\s*=\s*(\d+)',block)}

def parse(payload):
    if len(payload)<3:raise ValueError('Truncated StartSound')
    ident,flags=struct.unpack_from('<HB',payload);p=3
    if flags&0xf0:raise ValueError('Unsupported synchronization/reserved flags')
    def take(fmt):
        nonlocal p
        size=struct.calcsize(fmt)
        if p+size>len(payload):raise ValueError('Truncated SoundInfo')
        value=struct.unpack_from(fmt,payload,p);p+=size;return value
    start=take('<I')[0] if flags&1 else 0
    end=take('<I')[0] if flags&2 else None
    loops=take('<H')[0] if flags&4 else 1
    env=[take('<IHH') for _ in range(take('<B')[0])] if flags&8 else []
    if p!=len(payload) or loops!=1 or (end is not None and end<=start):raise ValueError('Invalid bounds or unsupported loops')
    if any(a[0]>=b[0] for a,b in zip(env,env[1:])) or any(max(l,r)>32768 for _,l,r in env):raise ValueError('Invalid envelope')
    return {'source_id':ident,'in_sample':start,'out_sample':end,'envelope':env}

def catalog(original,supported):
    payloads=sorted({x for ev in original.timelines.values() for _,t,x in ev if t==15 and struct.unpack_from('<H',x)[0] in supported and x[2]})
    if len(payloads)>MAX_EVENTS:raise ValueError('Too many sound variants')
    return {raw:dict(parse(raw),id=BASE_ID+i) for i,raw in enumerate(payloads)}

def bake(pcm,event,is_stereo,output_rate=22050):
    import numpy as np
    samples=np.frombuffer(pcm,dtype='<i2').reshape(-1,2)
    start=event['in_sample']*output_rate//44100
    end=len(samples) if event['out_sample'] is None else min(len(samples),event['out_sample']*output_rate//44100)
    if not 0<=start<end:raise ValueError('SoundInfo selects no decoded samples')
    out=samples[start:end].astype(np.float64)
    env=event['envelope']
    if env:
        # Envelope time begins when this event starts, independently of trim.
        positions=np.arange(len(out),dtype=np.float64)*44100/output_rate
        pts=np.asarray(env,dtype=np.float64)
        if is_stereo:
            for c in range(2):out[:,c]*=np.interp(positions,pts[:,0],pts[:,c+1])/32768
        else:out*= (np.interp(positions,pts[:,0],(pts[:,1]+pts[:,2])/2)/32768)[:,None]
    channels=2 if is_stereo else 1
    if channels==1:out=out.mean(axis=1)
    return np.rint(out).clip(-32768,32767).astype('<i2').tobytes(),channels

def decode(original,ident,recovered,ffmpeg):
    """Respect DefineSound's MP3 latency and authored sample count."""
    import numpy as np
    tag,data=original.payloads[ident]
    if tag!=14 or data[2]>>4!=2:raise ValueError('Expected original MP3')
    rate=[5512,11025,22050,44100][(data[2]>>2)&3]
    raw=subprocess.check_output([str(ffmpeg),'-v','error','-nostdin','-i',str(Path(recovered)/'audio'/f'{ident:04d}.mp3'),'-ac','2','-ar','22050','-f','s16le','pipe:1'])
    a=np.frombuffer(raw,dtype='<i2').reshape(-1,2)
    seek=struct.unpack_from('<h',data,7)[0]*22050//rate
    a=a[seek:] if seek>=0 else np.pad(a,((-seek,0),(0,0)))
    count=struct.unpack_from('<I',data,3)[0]*22050//rate
    if not count:raise ValueError('Empty authored sound')
    if len(a)<count:a=np.pad(a,((0,count-len(a)),(0,0)))
    return a[:count].astype('<i2').tobytes()

def produce(original,recovered,stage,ffmpeg,supported):
    events=catalog(original,supported);decoded={};rows=[];total=0
    for raw,event in events.items():
        ident=event['source_id'];tag,data=original.payloads[ident]
        if tag!=14 or data[2]>>4!=2:raise ValueError('Expected original MP3')
        stereo=bool(data[2]&1)
        if ident not in decoded:
            decoded[ident]=decode(original,ident,recovered,ffmpeg)
        pcm,channels=bake(decoded[ident],event,stereo);total+=len(pcm)
        if len(pcm)>4*1024*1024 or total>MAX_BANK_BYTES:raise ValueError('Sound event bank exceeds 3DS budget')
        (Path(stage)/f"sfx_{event['id']:04d}.pcm").write_bytes(pcm)
        rows.append(dict(event,channels=channels,frames=len(pcm)//(2*channels),sound_info=raw.hex()))
    index=b'SFE1'+struct.pack('<I',len(rows))+b''.join(struct.pack('<HBBI',e['id'],e['channels'],0,e['frames']) for e in rows)
    (Path(stage)/'sfx_events.bin').write_bytes(index)
    (Path(stage)/'sfx_events.json').write_text(json.dumps({'schema':1,'bytes':total,'events':rows},indent=2)+'\n',encoding='utf-8')
    return rows

def validate_bank(rom):
    rom=Path(rom);p=rom/'sfx_events.bin'
    if not p.is_file():return set()
    data=p.read_bytes()
    if len(data)<8 or data[:4]!=b'SFE1':raise ValueError('Invalid sound bank header')
    count=struct.unpack_from('<I',data,4)[0]
    if count>MAX_EVENTS or len(data)!=8+count*8:raise ValueError('Invalid sound bank count')
    ids=set();total=0
    for ident,channels,reserved,frames in struct.iter_unpack('<HBBI',data[8:]):
        if ident<BASE_ID or ident in ids or channels not in (1,2) or reserved or not frames:raise ValueError('Invalid sound bank record')
        p=rom/f'sfx_{ident:04d}.pcm';size=p.stat().st_size if p.is_file() else 0
        if size!=frames*channels*2 or size>4*1024*1024:raise ValueError('Invalid PCM: '+str(p))
        total+=size;ids.add(ident)
    if total>MAX_BANK_BYTES:raise ValueError('Sound bank memory budget exceeded')
    return ids
