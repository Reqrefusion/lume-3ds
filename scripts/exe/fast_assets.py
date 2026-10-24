"""Lossless load-speed repacking, bounded by an explicit total size budget.

No video decode or neural inference. Existing LAN metadata, delta/reference
structure, RGB samples and alpha are preserved exactly. The budget applies
only to positive growth; savings never buy an unbounded larger file.
"""
from pathlib import Path
import sys,struct,zlib,os,json,hashlib
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE));sys.path.insert(0,str(HERE.parent))
DEFAULT_BUDGET=8*1024*1024

def dzp_decode(data):
 from compress import restore
 import lz4.block
 magic,w,h=struct.unpack_from('<4sII',data)
 if magic not in [b'DZP1',b'DZP2',b'DZP3'] or (w,h)!=(800,600):raise ValueError('Detail page header')
 mode=0 if magic==b'DZP1' else data[12];body=data[12 if magic==b'DZP1' else 13:]
 if magic==b'DZP3':raw=lz4.block.decompress(body,uncompressed_size=w*h*3)
 else:
  dec=zlib.decompressobj();raw=dec.decompress(body,w*h*3+1)
  if len(raw)!=w*h*3 or not dec.eof or dec.unused_data or dec.unconsumed_tail:raise ValueError('Detail page payload')
 if len(raw)!=w*h*3:raise ValueError('Detail page length')
 return restore(raw,w,3,mode)

def dzp_encode(raw):
 from compress import predict,restore
 import lz4.block
 choices=[]
 for mode in range(3):
  filtered=predict(raw,800,3,mode)
  body=lz4.block.compress(filtered,mode='high_compression',compression=9,store_size=False)
  choices.append(b'DZP3'+struct.pack('<II',800,600)+bytes([mode])+body)
 result=min(choices,key=len)
 if dzp_decode(result)!=raw:raise ValueError('Lossless page proof failed')
 return result

def lan_repack(data,budget):
 from lan_codec import index
 import lz4.block
 info=index(data);magic,w,h,fps,n,ix=info
 packets=[];converted=skipped=growth=0
 for i in range(n):
  a,b=ix[i:i+2];packet=data[a:b];tag,x,y,cw,ch=struct.unpack_from('<IhhHH',packet)
  # References contain an index, not a compressed payload. Empty crops are
  # retained verbatim. Delta bytes must not be restored/requantized here.
  if cw*ch and not(tag&0x80000000) and not(magic==b'LAN5' and tag&(1<<27)):
   dec=zlib.decompressobj();raw=dec.decompress(packet[12:],cw*ch*4+1)
   if len(raw)!=cw*ch*4 or not dec.eof or dec.unconsumed_tail or dec.unused_data:raise ValueError('LAN zlib bounds')
   body=lz4.block.compress(raw,mode='high_compression',compression=9,store_size=False)
   extra=max(0,len(body)+12-len(packet))
   if extra<=budget-growth:
    if lz4.block.decompress(body,uncompressed_size=len(raw))!=raw:raise ValueError('LAN exact filtered bytes')
    packet=struct.pack('<IhhHH',tag|0x80000000,x,y,cw,ch)+body;growth+=extra;converted+=1
   else:skipped+=1
  packets.append(packet)
 # LAN2 did not distinguish codecs; LAN3 adds precisely that flag. Other
 # format bits/metadata and the complete original temporal topology remain.
 if magic==b'LAN2' and converted:magic=b'LAN3'
 offsets=[20+4*(n+1)]
 for p in packets:offsets.append(offsets[-1]+len(p))
 return struct.pack('<4s4I',magic,w,h,fps,n)+struct.pack('<%dI'%(n+1),*offsets)+b''.join(packets),growth,converted,skipped

def build(rom,budget=DEFAULT_BUDGET):
 from state import output_files
 rom=Path(rom);remaining=budget;changes={};records=[]
 pages=sorted(rom.rglob('*.dzp'))
 lans=sorted([p for p in output_files(rom) if p.suffix in ['.lan','.lta']],key=lambda p:('detail' in p.relative_to(rom).parts,p.name))
 policy=rom/'media_runtime_budget.json'
 try:previous_policy=json.loads(policy.read_text(encoding='utf-8')).get('files',{})
 except (OSError,ValueError,AttributeError):previous_policy={}
 if not isinstance(previous_policy,dict):previous_policy={}
 tracked={}
 for name,entry in previous_policy.items():
  p=rom/name
  if '..' in Path(name).parts or Path(name).is_absolute() or not p.is_file():continue
  if isinstance(entry,dict) and entry.get('sha256')==hashlib.sha256(p.read_bytes()).hexdigest():tracked[name]=entry
 reserved=sum(max(0,int(v.get('growth',0))) for v in tracked.values())
 remaining=max(0,budget-reserved)
 try:
  for p in pages+lans:
   old=p.read_bytes()
   if p.suffix=='.dzp':
    if old[:4]==b'DZP3':continue
    new=dzp_encode(dzp_decode(old));extra=max(0,len(new)-len(old));converted=1;skipped=0
    if extra>remaining:continue
   else:new,extra,converted,skipped=lan_repack(old,remaining)
   if new==old:continue
   remaining-=extra;changes[p]=old;temp=p.with_name(p.name+'.tmp');temp.write_bytes(new);os.replace(temp,p)
   name=p.relative_to(rom).as_posix()
   tracked[name]={'sha256':hashlib.sha256(new).hexdigest(),'growth':int(tracked.get(name,{}).get('growth',0))+extra}
   records.append({'file':name,'before':len(old),'after':len(new),'fast_blocks_added':converted,'blocks_kept_for_budget':skipped})
   print('Fast lossless asset:',p.name,len(old),'->',len(new),flush=True)
  policy_data=(json.dumps({'format':1,'files':tracked},sort_keys=True,indent=2)+'\n').encode('utf-8')
  old_policy=policy.read_bytes() if policy.is_file() else None
  if old_policy!=policy_data:
   changes[policy]=old_policy;policy.write_bytes(policy_data)
 except Exception:
  for p,old in changes.items():
   if old is None:p.unlink(missing_ok=True)
   else:p.write_bytes(old)
  raise
 return {'growth_budget':budget,'existing_growth_reserved':reserved,'positive_growth_spent':max(0,budget-reserved)-remaining,'net_change':sum(r['after']-r['before'] for r in records),'video_conversion':False,'lossless':True,'records':records},changes

def run(rom):return build(rom)[0]

def main():
 from state import Lock,write_json
 root=HERE.parents[1];rom=root/'romfs'
 if not rom.is_dir():raise ValueError('Generate game assets first.')
 sys.path.insert(0,str(root/'build'));from manifest import save
 with Lock(root/'tools/exe-production/production.lock'):
  metadata={p:p.read_bytes() if p.is_file() else None for p in [root/'assets.sha256.json',root/'source-manifest.json']}
  report,changes=build(rom)
  try:save(root)
  except Exception:
   for p,old in changes.items():
    if old is None:p.unlink(missing_ok=True)
    else:p.write_bytes(old)
   for p,old in metadata.items():
    if old is None:p.unlink(missing_ok=True)
    else:p.write_bytes(old)
   raise
  write_json(root/'logs/fast-assets.json',report)
 print('Fast lossless assets ready; no video conversion. Run BUILD.cmd.')
if __name__=='__main__':
 try:main()
 except Exception as exc:print('ERROR:',exc);raise SystemExit(1)
