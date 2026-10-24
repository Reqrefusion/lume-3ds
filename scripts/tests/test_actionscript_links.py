"""Keep archived AVM1 evidence and native source references reviewable.
These checks detect drift; they do not prove complete AVM1 equivalence.
"""
from pathlib import Path
import hashlib,json,re,unittest
ROOT=Path(__file__).resolve().parents[2]
class ActionScriptLinkTests(unittest.TestCase):
 def test_snapshot_and_binding_locations(self):
  manifest=json.loads((ROOT/'docs/actionscript-mapping.json').read_text(encoding='utf-8'))
  self.assertEqual(len(manifest['snapshot']),282)
  for row in manifest['snapshot']:
   with self.subTest(path=row['path']):
    self.assertEqual(hashlib.sha256((ROOT/row['path']).read_bytes()).hexdigest(),row['sha256'])
  original=ROOT/manifest['original_swf']['path']
  self.assertEqual(hashlib.sha256(original.read_bytes()).hexdigest(),manifest['original_swf']['sha256'])
  for row in manifest['bindings']:
   with self.subTest(binding=row['id']):
    source=(ROOT/row['source']).read_text(encoding='utf-8-sig')
    self.assertIn(row['source_marker'],source)
    self.assertEqual(source[:source.index(row['source_marker'])].count('\n')+1,row['source_line'])
    self.assertIn(row['native_symbol'],(ROOT/row['native']).read_text(encoding='utf-8-sig'))
 def test_solution_arrays_match_actionscript(self):
  native=(ROOT/'source/game/lume/puzzles.c').read_text()
  archive=ROOT/'docs/actionscript'
  for sprite,source_name,native_name in [(515,'linkArrayCorrect','lume_wire_solution'),(738,'numberArray','lume_piano_solution')]:
   source=(archive/f'DefineSprite_{sprite}/frame_1/DoAction.as').read_text()
   expected=list(map(int,re.search(source_name+r'\s*=\s*\[([^]]+)\]',source)[1].split(',')))
   value=re.search(native_name+r'[^=]*=\s*\{([^}]+)\}',native)[1]
   self.assertEqual(list(map(int,value.split(','))),expected)
  for sprite,name,count in [(553,'lume_cupboard_codes',9),(709,'lume_door_code',3)]:
   source=(archive/f'DefineSprite_{sprite}/frame_1/DoAction.as').read_text()
   expected=list(map(int,re.findall(r'target[123] = (\d+);',source)))
   tail=native[native.index(name):].split(';',1)[0].split('=',1)[1]
   self.assertEqual(list(map(int,re.findall(r'\d+',tail))),expected)
   self.assertEqual(len(expected),count)
if __name__=='__main__':unittest.main()
