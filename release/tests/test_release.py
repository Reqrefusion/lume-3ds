"""Check the current geometry exporter and source-distribution boundary."""
from pathlib import Path
import importlib.util,struct,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'release'))
import export_maps,build_release

class ReleaseTests(unittest.TestCase):
 def test_video_exports_disparity_without_colour(self):
  with tempfile.TemporaryDirectory() as folder:
   source=Path(folder)/'source.lvs';target=Path(folder)/'maps.ldp'
   colour=b'ORIGINAL_COLOUR_MUST_NOT_BE_DISTRIBUTED';depth=b'geometry-only-test'
   packets=[struct.pack('<IHHII',0,2,2,len(colour),len(depth))+colour+depth,struct.pack('<IHHII',0,2,1,0,0)]
   start=20+4*3;offsets=[start,start+len(packets[0]),start+sum(map(len,packets))]
   source.write_bytes(struct.pack('<4sHHIII',b'LVM1',320,240,25,2,2)+struct.pack('<3I',*offsets)+b''.join(packets))
   report=export_maps.video(source,target);data=target.read_bytes()
   self.assertEqual(report['frames'],2);self.assertEqual(report['colour_bytes_copied'],0)
   self.assertNotIn(colour,data);self.assertIn(depth,data);self.assertEqual(data[:4],b'LDP1')
   ix=struct.unpack_from('<3I',data,20)
   self.assertEqual(struct.unpack_from('<IHBI',data,ix[0]),(0,2,2,len(depth)))
   self.assertEqual(struct.unpack_from('<IHBI',data,ix[1]),(0,2,1,0))
 def test_truncated_video_is_rejected(self):
  with tempfile.TemporaryDirectory() as folder:
   source=Path(folder)/'source.lvs';source.write_bytes(struct.pack('<4sHHIII',b'LVM1',320,240,25,1,1)+struct.pack('<2I',28,100))
   with self.assertRaises(ValueError):export_maps.video(source,Path(folder)/'maps.ldp')
 def test_source_export_contains_tools_but_no_private_assets(self):
  with tempfile.TemporaryDirectory() as folder:
   target=Path(folder)/'source';manifest=build_release.export_source(target)
   for name in ['scripts/exe/pipeline.py','build/produce.ps1','source/main.c','release/export_maps.py','docs/BUILDING.md','.github/workflows/check.yml','SETUP.cmd','release/setup_player.ps1','player/lume.3dsx','player/runtime.json']:
    self.assertIn(name,manifest)
   forbidden={'.exe','.swf','.3dsx','.l3dp','.rgb','.rgba','.lvs','.lan','.pcm','.mp3','.o','.a','.class'}
   self.assertFalse(any(Path(name).suffix in forbidden and name!='player/lume.3dsx' for name in manifest))
   for name in export_maps.prepared_files(ROOT):
    self.assertIn(name,manifest)
   self.assertTrue(all(name.startswith('maps/') for name in manifest if Path(name).suffix in {'.sdf','.depth','.tsf','.ldp'}))
   import json
   self.assertTrue(json.loads((target/'languages/tr.json').read_text(encoding='utf-8'))['text']['bladesTaken'])
   self.assertFalse((target/'release/installer').exists())
   self.assertFalse((target/'source/game/lume/original_font.h').exists())
   self.assertEqual((target/'docs/BUILDING.md').read_bytes(),(ROOT/'docs/BUILDING.md').read_bytes())
class PreparedMapTests(unittest.TestCase):
 def test_corrupt_map_is_rejected(self):
  with tempfile.TemporaryDirectory() as folder:
   root=Path(folder);maps=root/'maps';maps.mkdir();data=b'SDF1'+bytes(8)
   path=maps/'sample.sdf';path.write_bytes(data)
   import hashlib,json
   (maps/'manifest.json').write_text(json.dumps({'content':'geometry-only','files':{'maps/sample.sdf':{'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}}}))
   (maps/'README.md').write_text('Geometry only')
   export_maps.prepared_files(root)
   path.write_bytes(data[:-1]+b'X')
   with self.assertRaises(ValueError):export_maps.prepared_files(root)
if __name__=='__main__':unittest.main()
