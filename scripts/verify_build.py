"""Read-only source/media regression entry point; does not regenerate media."""
from pathlib import Path
import sys,json,unittest
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'build'));sys.path.insert(0,str(ROOT/'scripts/exe'))
from manifest import verify,source_inventory,sha
from romfs_compact import inventory as embedded_inventory
from contract import validate

def main():
 assets=verify(ROOT);report=validate(ROOT,ROOT/'romfs')
 if not report['complete_game_assets']:raise ValueError(json.dumps(report,ensure_ascii=False))
 suite=unittest.defaultTestLoader.discover(str(ROOT/'scripts/tests'))
 result=unittest.TextTestRunner(verbosity=2).run(suite)
 if not result.wasSuccessful() or result.skipped:return 1
 info=json.loads((ROOT/'out/build-report.json').read_text(encoding='utf-8-sig'))
 current=(info.get('source')==source_inventory(ROOT) and info.get('assets')==assets and info.get('sha256')==sha(ROOT/'out/lume.3dsx'))
 embedded_ok=embedded_inventory(ROOT/'out/lume.3dsx')==assets
 print('Packaged assets match the source exactly:',embedded_ok)
 current=current and embedded_ok
 print('Asset contract:',report['total_files'],'files, no errors')
 print('Current 3DSX matches current source and assets:',current)
 return 0 if current else 2
if __name__=='__main__':raise SystemExit(main())
