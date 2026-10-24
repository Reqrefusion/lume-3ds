"""Fast pre-build regression gate; never regenerates game media."""
from pathlib import Path
import sys,unittest
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]

def main():
 tests=ROOT/'scripts/tests'
 sys.path.insert(0,str(tests))
 suite=unittest.defaultTestLoader.discover(str(tests))
 if not suite.countTestCases():
  print('ERROR: Regression tests not found.');return 1
 result=unittest.TextTestRunner(verbosity=2).run(suite)
 if result.skipped:
  print('ERROR: Quality checks incomplete because tests were skipped. Native C tests require MSVC or cc.')
 return 0 if result.wasSuccessful() and not result.skipped else 1
if __name__=='__main__':raise SystemExit(main())
