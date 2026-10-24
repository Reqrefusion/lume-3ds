"""Locally derive a fallback atlas after full EXE production, if it is missing."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
if __name__=='__main__':
 out=ROOT/'source/game/lume/original_font.h'
 if not out.exists():
  b=(ROOT/'romfs/detail/native_menu/font.bin').read_bytes()
  if b[:8]!=b'LRF1\x0c\x14\x5f\x00' or len(b)!=8+95*240:
   raise ValueError('Unexpected original glyph atlas')
  glyphs=[]
  for g in range(95):
   a=b[8+g*240:8+(g+1)*240];glyphs.append([(a[y*24+x*2]+a[y*24+x*2+1]+a[y*24+x*2+12]+a[y*24+x*2+13]+2)//4 for y in range(10) for x in range(6)])
  text='#ifndef LUME_ORIGINAL_FONT_H\n#define LUME_ORIGINAL_FONT_H\nstatic const unsigned char lume_font_alpha[95][60]={\n'+',\n'.join('{'+','.join(map(str,g))+'}' for g in glyphs)+'\n};\n#endif\n'
  out.write_text(text,encoding='ascii');print('Local fallback font derived from owned game assets.')
