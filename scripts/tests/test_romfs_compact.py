"""Exercise actual RomFS packing with same-size different bytes and shared data."""
from pathlib import Path
import importlib.util,struct,tempfile,unittest
from test_fence_flow import ROOT
spec=importlib.util.spec_from_file_location('romfs_compact',ROOT/'build/romfs_compact.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
def fixture(path):
 names=['first.bin','same.bin','different.bin','empty.bin'];data=[b'abcd'*64,b'abcd'*64,b'efgh'*64,b'']
 table=bytearray();payload=bytearray()
 for name,content in zip(names,data):
  raw=name.encode('utf-16le');table.extend(struct.pack('<IIQQII',0,0xffffffff,len(payload),len(content),0xffffffff,len(raw))+raw)
  table.extend(b'\0'*((-len(table))%4));payload.extend(content)
 # Two hash tables and the root dir are retained unchanged by compaction.
 start=40+4+24+4+len(table)
 meta=struct.pack('<10I',40,40,4,44,24,68,4,72,len(table),start)+b'\xff'*4+struct.pack('<6I',0,0xffffffff,0xffffffff,0,0xffffffff,0)+b'\xff'*4+table
 prefix=bytearray(64);prefix[:4]=b'3DSX';struct.pack_into('<H',prefix,4,44);struct.pack_into('<I',prefix,40,64)
 path.write_bytes(prefix+meta+payload)
class RomfsCompactTests(unittest.TestCase):
 def test_names_bytes_offsets_and_idempotence(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'work') as folder:
   src=Path(folder)/'source.3dsx';dst=Path(folder)/'small.3dsx';again=Path(folder)/'again.3dsx';fixture(src)
   original=src.read_bytes();expected=m.inventory(src);report=m.compact_3dsx(src,dst,expected)
   self.assertGreater(report['saved_bytes'],0);self.assertEqual(report['shared_entries'],1)
   self.assertEqual(m.inventory(dst),expected);self.assertEqual(src.read_bytes(),original)
   _,_,_,files=m.read_index(dst);offsets={name:offset for name,offset,_,_ in files}
   self.assertEqual(offsets['first.bin'],offsets['same.bin']);self.assertNotEqual(offsets['first.bin'],offsets['different.bin'])
   self.assertEqual(dst.read_bytes()[:64],original[:64])
   m.compact_3dsx(dst,again,expected);self.assertEqual(dst.read_bytes(),again.read_bytes())
 def test_invalid_source_does_not_replace_output(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'work') as folder:
   src=Path(folder)/'source.3dsx';dst=Path(folder)/'small.3dsx';fixture(src);dst.write_bytes(b'keep previous')
   expected=m.inventory(src);expected['first.bin']['sha256']='0'*64
   with self.assertRaises(ValueError):m.compact_3dsx(src,dst,expected)
   self.assertEqual(dst.read_bytes(),b'keep previous')
   with self.assertRaises(ValueError):m.compact_3dsx(src,src)
 def test_truncated_payload_is_rejected(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'work') as folder:
   src=Path(folder)/'source.3dsx';fixture(src);src.write_bytes(src.read_bytes()[:-1])
   with self.assertRaises(ValueError):m.read_index(src)
if __name__=='__main__':unittest.main()
