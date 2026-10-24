"""Compile using imported ready assets. Does not invoke Java, FFmpeg or depth models."""
from pathlib import Path
import sys,argparse
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'build'));sys.path.insert(0,str(ROOT/'scripts/exe'))
from build_game import build
from state import Lock
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--jobs',type=int,default=0);a=p.parse_args()
 with Lock(ROOT/'tools/exe-production/production.lock'):build(ROOT,a.jobs)
