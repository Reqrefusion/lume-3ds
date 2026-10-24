"""Fixed app, external assets and safe updates without a native toolchain."""
from pathlib import Path
import hashlib,json,os,struct,sys,tempfile,unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'release'))
import setup_player,player_content

class PlayerSetupTests(unittest.TestCase):
 def test_finds_supported_steam_file_and_rejects_wrong_explicit_file(self):
  with tempfile.TemporaryDirectory() as folder:
   root=Path(folder);steam=root/'Steam/steamapps/common/Lume';steam.mkdir(parents=True);path=steam/'Lume.exe';path.write_bytes(b'owned test original')
   with patch.dict(os.environ,{'ProgramFiles(x86)':str(root)}),patch.object(setup_player,'EXPECTED_EXE',hashlib.sha256(path.read_bytes()).hexdigest()):
    self.assertEqual(setup_player.find_original(root),path.resolve())
    wrong=root/'wrong.exe';wrong.write_bytes(b'wrong')
    with self.assertRaises(ValueError):setup_player.find_original(root,wrong)
 def test_fixed_app_has_no_embedded_game(self):
  app=(ROOT/'player/lume.3dsx').read_bytes();meta=json.loads((ROOT/'player/runtime.json').read_text())
  self.assertEqual(app[:4],b'3DSX');self.assertEqual(struct.unpack_from('<I',app,40)[0],0)
  self.assertEqual(hashlib.sha256(app).hexdigest(),meta['sha256']);self.assertEqual(meta['storage'],'external_sd')
  self.assertFalse(meta['original_game_content_embedded']);self.assertTrue(meta['original_logo_included']);self.assertFalse(meta['original_font_included'])
 def test_updates_external_content_preserving_state_and_fixed_app(self):
  with tempfile.TemporaryDirectory() as folder:
   root=Path(folder);content=root/'content';content.mkdir();asset=content/'scene.bin';asset.write_bytes(b'first')
   output=root/'out';app=ROOT/'player/lume.3dsx'
   def expected():return {'scene.bin':{'bytes':asset.stat().st_size,'sha256':player_content.digest(asset)}}
   binary=player_content.publish(app,content,output,expected(),{})
   self.assertEqual(binary,output/'3ds/lume.3dsx');self.assertEqual(binary.read_bytes(),app.read_bytes())
   stamp=binary.stat().st_mtime_ns;data=output/'3ds/lume'
   for name in ['save.lum','save.lum.bak','settings.cfg','runtime.log','disable_stereo.flag']:(data/name).write_bytes(name.encode())
   asset.write_bytes(b'second');player_content.publish(app,content,output,expected(),{})
   self.assertEqual(binary.stat().st_mtime_ns,stamp);self.assertEqual((data/'scene.bin').read_bytes(),b'second')
   for name in ['save.lum','save.lum.bak','settings.cfg','runtime.log','disable_stereo.flag']:self.assertEqual((data/name).read_bytes(),name.encode())
   broken=expected();broken['scene.bin']['sha256']='0'*64
   with self.assertRaises(ValueError):player_content.publish(app,content,output,broken,{})
   self.assertEqual((data/'scene.bin').read_bytes(),b'second');self.assertEqual(binary.stat().st_mtime_ns,stamp)
   with self.assertRaises(ValueError):player_content.publish(app,content,output,{'../escape':expected()['scene.bin']},{})
   with self.assertRaises(ValueError):player_content.publish(app,content,output,{'save.lum':expected()['scene.bin']},{})
if __name__=='__main__':unittest.main()