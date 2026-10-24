"""SDK discovery supports standard devkitPro and existing portlibs layouts."""
from pathlib import Path
import os,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'build'))
from build_game import sdk_tools

class SdkLayoutTests(unittest.TestCase):
 def fixture(self,root,citro_folder):
  base=root/'tools/sdk/opt/devkitpro';ext='.exe' if os.name=='nt' else ''
  names=['devkitARM/bin/arm-none-eabi-gcc'+ext,'tools/bin/3dsxtool'+ext,'tools/bin/smdhtool'+ext,'libctru/lib/libctru.a']
  names += ['portlibs/3ds/lib/lib'+n+'.a' for n in ['mpg123','jpeg','lz4','z']]
  names += [citro_folder+'/lib/lib'+n+'.a' for n in ['citro2d','citro3d']]
  for name in names:
   path=base/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b'fixture, never executed')
  return base
 def test_standard_sdk_keeps_citro_with_libctru(self):
  with tempfile.TemporaryDirectory() as folder:
   root=Path(folder);base=self.fixture(root,'libctru');self.assertEqual(sdk_tools(root)['base'],base)
 def test_existing_portlibs_citro_install_remains_supported(self):
  with tempfile.TemporaryDirectory() as folder:
   root=Path(folder);base=self.fixture(root,'portlibs/3ds');self.assertEqual(sdk_tools(root)['base'],base)
 def test_missing_citro_reports_both_searched_locations(self):
  with tempfile.TemporaryDirectory() as folder:
   root=Path(folder);base=self.fixture(root,'libctru');(base/'libctru/lib/libcitro2d.a').unlink()
   with self.assertRaises(ValueError) as caught:sdk_tools(root)
   self.assertIn(str(base/'libctru/lib/libcitro2d.a'),str(caught.exception));self.assertIn(str(base/'portlibs/3ds/lib/libcitro2d.a'),str(caught.exception))
if __name__=='__main__':unittest.main()