"""Check the exported product, not just the in-tree game configuration."""
from pathlib import Path
import importlib.util,json,re,tempfile,unittest,hashlib
from test_fence_flow import ROOT
class EngineExportTests(unittest.TestCase):
 def test_independent_export_and_profile_boundaries(self):
  spec=importlib.util.spec_from_file_location('engine_export',ROOT/'scripts/exe/export_engine.py')
  module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
  source=ROOT/'source';work=ROOT/'work';work.mkdir(exist_ok=True)
  with tempfile.TemporaryDirectory(prefix='export_test_',dir=work) as folder:
   dest=module.export(source,Path(folder)/'engine')
   self.assertFalse((dest/'game').exists());self.assertFalse((dest/'main.c').exists())
   self.assertFalse((dest/'romfs').exists())
   profile=json.loads((dest/'runtime-profile.json').read_text(encoding='utf-8'))
   self.assertEqual((profile['media_top'],profile['media_bottom']),(0,240))
   for path in (dest/'engine').rglob('*.c'):
    text=path.read_text(encoding='utf-8')
    self.assertNotIn('#define LUME_MEDIA_TOP 13',text)
    self.assertNotIn('game/lume',text)
   gpu=(dest/'platform/3ds/video_present.c').read_text(encoding='utf-8')
   self.assertIn('LUME_MEDIA_BOTTOM-LUME_MEDIA_TOP',gpu)
   self.assertNotIn('rgb,13,height',gpu)
   builder=(dest/'build_3ds.py').read_text(encoding='utf-8')
   compile(builder,str(dest/'build_3ds.py'),'exec')
   self.assertNotIn('build_game',builder);self.assertNotIn('Lume.exe',builder)
   self.assertTrue((dest/'examples/stereo_demo.c').is_file())
   self.assertTrue((dest/'romfs_compact.py').is_file())
   self.assertIn('sys.path.insert(0,str(ROOT))',builder)
   manifest=json.loads((dest/'export-manifest.json').read_text(encoding='utf-8'))
   for name,sha in manifest.items():self.assertEqual(hashlib.sha256((dest/name).read_bytes()).hexdigest(),sha)
   with self.assertRaises(ValueError):module.export(source,dest)
  with self.assertRaises(ValueError):module.export(source,source/'invalid_export')
if __name__=='__main__':unittest.main()
