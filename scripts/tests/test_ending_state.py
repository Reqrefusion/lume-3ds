"""Exercise the real SWF exporter final-state policy and rendered rotor motion."""
from pathlib import Path
import os,subprocess,unittest
from test_fence_flow import ROOT
class EndingStateTests(unittest.TestCase):
 def test_completed_turbine_fence_and_speaker_tail(self):
  java=next((ROOT/'tools/detail/java').rglob('java.exe'));jar=ROOT/'tools/detail/ffdec/ffdec.jar'
  cp=os.pathsep.join([str(ROOT/'scripts/java'),str(jar),str(jar.parent/'lib/*')])
  p=subprocess.run([str(java),'-Xmx768m','-Djava.awt.headless=true','-cp',cp,'EndingStateCheck',str(ROOT/'tools/exe-production/recovered/Lume.swf'),str(ROOT/'tools/exe-production/motion/requests')],cwd=ROOT,capture_output=True,text=True,errors='replace',timeout=60)
  self.assertEqual(p.returncode,0,p.stdout+p.stderr);self.assertIn('rotor phases=48',p.stdout);print(p.stdout.strip())
if __name__=='__main__':unittest.main()
