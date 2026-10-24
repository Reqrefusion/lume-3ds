"""Explicit authored frame selection; no estimated movement or input media."""
from pathlib import Path
import struct,json
from original_swf import Original,u16
from recover import actions
from sound_events import catalog,supported_effects
NAV={'nav_0_1':(12,131),'nav_1_0':(149,274),'nav_1_2':(276,399),'nav_2_1':(411,534),'nav_2_3':(570,699),'nav_3_2':(714,844),'nav_3_4':(846,969),'nav_4_3':(981,1115),'nav_4_1':(1117,1255),'nav_1_4':(1257,1435),'nav_3_5':(1438,1603),'nav_5_3':(1617,1749),'nav_hall_stairs_after_walk':(869,969)}
WALKS={'walk_roof_ab':(132,806,list(range(11,33)),19),'walk_roof_ba':(132,806,list(range(170,188))+[1],19),'walk_roof_bg':(132,806,list(range(188,198)),19),'walk_front_ab':(400,826,list(range(11,64)),19),'walk_front_ba':(400,826,list(range(64,114))+[1],19),'walk_front_ad':(400,826,list(range(114,125)),19),'walk_front_bd':(400,826,list(range(126,182)),19),'walk_hall_ba':(701,855,list(range(8,46)),28),'walk_hall_ab':(701,855,list(range(55,93))+[1],28),'walk_hall_as':(701,855,list(range(93,104)),28),'anim_gramophone':(970,886,list(range(11,96))+[1],38)}
MOVIES={175:0,740:1,940:2,945:3};LIMITS=[1602,149,199,409]

def make(swf,dest):
 dest=Path(dest);dest.mkdir(parents=True,exist_ok=True);o=Original(swf);states=o.states(988);plates={};summary={}
 event_map=catalog(o,supported_effects(Path(__file__).resolve().parents[2]/'source'))
 def backdrop(fr):
  # A photographed plate can be above actors during a dissolve. Only plates
  # below the first foreground layer belong in the decoded background. Keep
  # later plates in the RGBA stream, at their original display-list depths.
  st=states[fr]
  front=[d for d,e in st.items() if e['character'] not in MOVIES and e['character'] not in (910,915,917,753) and not e.get('name','').startswith(('hotspot','btn'))]
  first=min(front,default=65536)
  return {d:e for d,e in st.items() if e['character'] in (910,915,917) and d<first}
 def background(fr,walking=False):
  st=states[fr];e=st.get(117,{}) if walking else st.get(1,{})
  if e.get('character') not in MOVIES:e=st.get(117,{})
  if e.get('character') not in MOVIES:e=st.get(116,{})
  if any(e['character'] in (910,915,917) for e in st.values()):
   p=backdrop(fr);key=repr(sorted(p.items()))
   if key not in plates:plates[key]=(len(plates),fr,','.join(map(str,sorted(p))) or '-')
   return 4,plates[key][0]
  if e.get('character') in MOVIES:
   mid=MOVIES[e['character']];return (mid,min(e.get('ratio',0),LIMITS[mid]))
  raise ValueError('No authored camera/plate at source frame '+str(fr))
 def cues(cid,fr):return [event_map[x]['id'] if x in event_map else u16(x) for f,t,x in o.timelines[cid] if f==fr and t==15]
 def output(name,mode,frames,child=-1,minimum=0,cid=988):
  rows=[];events=[];transforms=[]
  for root_fr,child_fr in frames:
   mid,bg=background(root_fr,mode=='walk');rows.append((root_fr,child_fr,minimum,mid,bg,','.join(map(str,sorted(backdrop(root_fr)))) or '-'))
   events.append(cues(cid,child_fr+1 if child_fr>=0 else root_fr))
   for depth in [135,148,155]:transforms.extend(states[root_fr].get(depth,{}).get('matrix',[0,0,0,0,0,0]))
  (dest/(name+'.tsv')).write_text(mode+'\n'+''.join('\t'.join(map(str,row))+'\n' for row in rows), encoding="utf-8")
  offsets=[0];ids=[]
  for es in events:ids+=es;offsets.append(len(ids))
  (dest/(name+'.sf2')).write_bytes(b'SFX2'+struct.pack('<II',len(rows),len(ids))+struct.pack('<%dI'%len(offsets),*offsets)+(struct.pack('<%dH'%len(ids),*ids) if ids else b''))
  if mode=='nav':(dest/(name+'.trf')).write_bytes(b'TRF1'+struct.pack('<II',len(rows),3)+struct.pack('<%df'%len(transforms),*transforms))
  summary[name]={'frames':len(rows),'source':[[r,c] for r,c in frames],'mode':mode}
 for name,(a,b) in NAV.items():output(name,'nav',[(f,-1) for f in range(a,b+1)])
 for name,(cam,cid,frames,m) in WALKS.items():output(name,'walk',[(cam,f-1) for f in frames],minimum=m,cid=cid)
 output('walk_front_try','nav',[(f,-1) for f in range(549,570)])
 for name,a,b in [('anim_intro',1752,2536),('anim_ending',2538,4445)]:output(name,'cinema',[(f,-1) for f in range(a,b+1)])
 (dest/'plates.tsv').write_text(''.join(str(i)+'\t'+str(fr)+'\t'+depths+'\n' for i,fr,depths in plates.values()), encoding="utf-8")
 stops=[]
 for cid,ev in o.timelines.items():
  last=max(f for f,t,x in ev if t==1) if any(t==1 for f,t,x in ev) else 0
  if any(t==12 and f==last and any(a['op']=='Stop' for a in actions(x)) for f,t,x in ev):stops.append((cid,last))
 (dest/'stops.tsv').write_text(''.join(str(cid)+'\t'+str(last)+'\n' for cid,last in stops), encoding="utf-8")
 (dest/'recipe.json').write_text(json.dumps(summary,indent=2)+'\n', encoding="utf-8")
 return summary
