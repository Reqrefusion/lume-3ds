"""Verify the authored logo plane and its closing-timeline scope."""
from pathlib import Path
import sys,re,unittest
import numpy as np
from test_fence_flow import ROOT
sys.path.insert(0,str(ROOT/'scripts/exe'))
from logo_depth import title_plane,quarter_pixels,POLICY
from assemble import frames
class LogoDepthTests(unittest.TestCase):
 def test_plane_preserves_outside_and_flat_word(self):
  source=np.full((120,160),-3,dtype=np.int8);original=source.copy();out=title_plane(source)
  self.assertTrue(np.all(out[22:49,38:123]==quarter_pixels()))
  self.assertTrue(np.array_equal(out[:10],source[:10]));self.assertTrue(np.array_equal(out[60:],source[60:]))
  self.assertTrue(np.array_equal(source,original));self.assertLessEqual(int(out.max()),quarter_pixels())
 def test_closing_logo_stays_inside_authored_region(self):
  first=int(re.search(r'#define LUME_LOGO_ENDING_FIRST_FRAME (\d+)',POLICY.read_text(encoding='utf-8')).group(1));checked=0
  for i,(_,image) in enumerate(frames(ROOT/'romfs/anim_ending.lan')):
   if i<first:continue
   rgba=np.array(image);visible=(rgba[:,:,:3].max(axis=2)>35)&(rgba[:,:,3]>20);y,x=np.where(visible)
   if len(x):
    self.assertGreaterEqual(int(x.min()),102);self.assertLess(int(x.max()),218)
    self.assertGreaterEqual(int(y.min()),18);self.assertLess(int(y.max()),62);checked+=1
  self.assertGreater(checked,0)
if __name__=='__main__':unittest.main()
