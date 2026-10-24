"""Complete translation coverage and the UI-only preparation route."""
from pathlib import Path
import json,shutil,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts'));sys.path.insert(0,str(ROOT/'release'))
from check_languages import check
from setup_player import preparation_arguments

class LanguageTests(unittest.TestCase):
 def test_every_supported_translation_covers_dialogue_documents_and_menus(self):
  result=check(ROOT/'languages')
  self.assertEqual(result['languages'],['de','en','es','tr']);self.assertEqual(result['authored_text_keys'],83)
  self.assertEqual(result['translated_nonempty_entries'],81);self.assertLessEqual(result['glyphs'],224)
 def test_a_missing_long_document_cannot_silently_fall_back_to_english(self):
  with tempfile.TemporaryDirectory() as folder:
   packs=Path(folder)/'languages';shutil.copytree(ROOT/'languages',packs)
   path=packs/'de.json';pack=json.loads(path.read_text(encoding='utf-8'));del pack['text']['doc_book3_7'];path.write_text(json.dumps(pack),encoding='utf-8')
   with self.assertRaisesRegex(ValueError,'story/document'):check(packs)
 def test_language_update_requests_only_ui_and_requires_existing_content(self):
  with tempfile.TemporaryDirectory() as folder:
   root=Path(folder)
   with self.assertRaisesRegex(ValueError,'once'):preparation_arguments(root,root/'original.exe',True)
   (root/'assets.sha256.json').write_text('{}')
   args=preparation_arguments(root,root/'original.exe',True);self.assertEqual(args[args.index('--stage')+1],'ui')
   args=preparation_arguments(root,root/'original.exe',False);self.assertEqual(args[args.index('--stage')+1],'all')
if __name__=='__main__':unittest.main()