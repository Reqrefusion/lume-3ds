"""Export the original close control as an independent bottom-screen UI layer."""
from pathlib import Path
import os,subprocess,tempfile,struct,sys
from PIL import Image

def produce(root,swf,rom,java,jar):
 root,rom=Path(root),Path(rom);sys.path.insert(0,str(root/'scripts'))
 import java_compile,toolchain
 java_compile.ensure(root,java,jar,toolchain.download)
 cp=os.pathsep.join([str(root/'scripts/java'),str(jar),str(Path(jar).parent/'lib/*')])
 with tempfile.TemporaryDirectory(prefix='close_ui_',dir=rom.parent) as folder:
  png=Path(folder)/'close.png'
  subprocess.run([str(java),'-Xmx512m','-Djava.awt.headless=true','-cp',cp,'CloseControlArt',str(swf),str(png)],cwd=root,check=True)
  with Image.open(png) as photo:
   canvas=Image.new('RGBA',(800,600));canvas.alpha_composite(photo.convert('RGBA'),(0,33))
   im=canvas.resize((320,240),Image.Resampling.LANCZOS);box=im.getbbox()
   if not box:raise ValueError('Empty close control')
   x,y,right,bottom=box;w,h=right-x,bottom-y
   if x<0 or right>29 or y<13 or bottom>44 or w>32 or h>32:raise ValueError('Close control bounds: '+str(box))
   data=struct.pack('<4sHHHH',b'XUI1',x,y,w,h)+im.crop(box).tobytes()
 (rom/'ui_close.bin').write_bytes(data);return len(data)
