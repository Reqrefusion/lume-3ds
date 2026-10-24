"""A cached package is reusable only with exact inputs and unmodified output."""
from pathlib import Path
import tempfile,json,sys,unittest,hashlib
from test_fence_flow import ROOT
sys.path.insert(0,str(ROOT/'build'))
from package_cache import signature,reusable
class PackageCacheTests(unittest.TestCase):
 def test_inputs_and_published_bytes(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'work') as folder:
   out=Path(folder);game=out/'game.3dsx';game.write_bytes(b'good');key=signature({'a':1,'b':2})
   self.assertEqual(key,signature({'b':2,'a':1}))
   info={'file':game.name,'build_input_signature':key,'sha256':hashlib.sha256(game.read_bytes()).hexdigest()}
   (out/'build-report.json').write_text(json.dumps(info))
   self.assertEqual(reusable(out,key),game);self.assertIsNone(reusable(out,signature({'a':2})))
   game.write_bytes(b'evil');self.assertIsNone(reusable(out,key))
   game.write_bytes(b'good');info['file']='../outside.3dsx';(out/'build-report.json').write_text(json.dumps(info))
   self.assertIsNone(reusable(out,key))
