"""Run with tools/python/python.exe -B -m unittest discover -s scripts/tests."""
from pathlib import Path
import sys,struct,tempfile,unittest
from unittest.mock import patch
import numpy as np
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts/exe'))
from sound_events import parse,bake,decode,catalog,validate_bank,supported_effects

class SoundInfoTests(unittest.TestCase):
 def test_trim_and_stereo_envelope_use_44100_clock(self):
  pcm=np.array([[1000,-1000],[2000,-2000],[3000,-3000],[4000,-4000]],dtype='<i2').tobytes()
  raw=struct.pack('<HBII',774,11,2,6)+struct.pack('<B',2)+struct.pack('<IHHIHH',0,32768,0,2,0,32768)
  event=parse(raw);out,channels=bake(pcm,event,True)
  self.assertEqual(channels,2)
  np.testing.assert_array_equal(np.frombuffer(out,'<i2').reshape(-1,2),[[2000,0],[0,-3000]])
 def test_mono_averages_envelope_channels(self):
  event={'in_sample':0,'out_sample':None,'envelope':[(0,32768,0)]}
  out,channels=bake(struct.pack('<4h',2000,2000,4000,4000),event,False)
  self.assertEqual(channels,1);self.assertEqual(struct.unpack('<2h',out),(1000,2000))
 def test_invalid_events_are_rejected(self):
  for raw in [b'',struct.pack('<HB',1,0x20),struct.pack('<HBH',1,1,0),struct.pack('<HBII',1,3,8,4)]:
   with self.subTest(raw=raw),self.assertRaises(ValueError):parse(raw)
 def test_mp3_latency_and_declared_length(self):
  # A 22050 Hz source has one-frame latency and exactly two authored frames.
  payload=struct.pack('<HBIh',7,0x2a,2,1)
  original=type('Original',(),{'payloads':{7:(14,payload)}})()
  pcm=struct.pack('<8h',100,100,200,200,300,300,400,400)
  with patch('sound_events.subprocess.check_output',return_value=pcm):out=decode(original,7,Path('.'),Path('ffmpeg'))
  self.assertEqual(struct.unpack('<4h',out),(200,200,300,300))
 def test_bank_checks_payload_size_and_duplicate_ids(self):
  with tempfile.TemporaryDirectory() as folder:
   p=Path(folder);index=p/'sfx_events.bin'
   index.write_bytes(b'SFE1'+struct.pack('<I',1)+struct.pack('<HBBI',32768,2,0,2))
   (p/'sfx_32768.pcm').write_bytes(b'\0'*8)
   self.assertEqual(validate_bank(p),{32768})
   index.write_bytes(b'SFE1'+struct.pack('<I',2)+struct.pack('<HBBI',32768,2,0,2)*2)
   with self.assertRaises(ValueError):validate_bank(p)
   index.write_bytes(b'SFE1'+struct.pack('<I',1)+struct.pack('<HBBI',32768,2,0,2))
   (p/'sfx_32768.pcm').write_bytes(b'\0'*6)
   with self.assertRaises(ValueError):validate_bank(p)
 def test_original_swf_catalog_is_deterministic(self):
  swf=ROOT/'tools/detail/Lume.swf'
  if not swf.is_file():self.skipTest('Original SWF not installed in this working copy')
  from original_swf import Original
  original=Original(swf);supported=supported_effects(ROOT/'source')
  first=catalog(original,supported);second=catalog(original,supported)
  self.assertEqual(first,second);self.assertTrue(first)
  # Music is owned by the MP3 service, never duplicated as an event effect.
  self.assertNotIn(946,{event['source_id'] for event in first.values()})
  event=next(event for raw,event in first.items() if raw.hex()=='06030a8070000001000000000c1e7523')
  self.assertEqual(event['source_id'],774);self.assertEqual(event['out_sample'],28800)

if __name__=='__main__':unittest.main()
