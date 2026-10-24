"""Reuse a published package only when its inputs and actual bytes still match."""
from pathlib import Path
import hashlib,json
from manifest import sha

def signature(inputs):
 return hashlib.sha256(json.dumps(inputs,sort_keys=True,separators=(',',':')).encode('utf-8')).hexdigest()

def reusable(out,key):
 out=Path(out)
 try:
  info=json.loads((out/'build-report.json').read_text(encoding='utf-8-sig'))
  name=info['file']
  if not isinstance(name,str) or Path(name).name!=name or '/' in name or '\\' in name or not name.endswith('.3dsx'):return None
  dest=out/name
  if info.get('build_input_signature')==key and dest.is_file() and sha(dest)==info.get('sha256'):return dest
 except (OSError,ValueError,KeyError,TypeError):pass
 return None
