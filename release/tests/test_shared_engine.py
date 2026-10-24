from pathlib import Path
import json,unittest
ROOT=Path(__file__).resolve().parents[2]
class SharedEngineTests(unittest.TestCase):
 def test_direct_profile_and_layout(self):
  self.assertTrue((ROOT/'source/engine').is_dir());self.assertFalse((ROOT/'engine').exists())
  for name in ['platform','features','adapters']:self.assertFalse((ROOT/'source'/name).exists())
  for path in json.loads((ROOT/'source/engine/modules.json').read_text())['lume']:self.assertTrue((ROOT/'source'/path).is_file())
