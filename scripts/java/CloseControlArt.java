import java.io.*;import java.nio.file.*;import java.awt.*;import java.awt.image.*;import javax.imageio.ImageIO;
import com.jpexs.decompiler.flash.SWF;import com.jpexs.decompiler.flash.exporters.commonshape.Matrix;
class CloseControlArt {
 public static void main(String[] args)throws Exception{
  NativePages.path=args[0];NativePages.setup(21);
  NativePages.pf.layers.entrySet().removeIf(e->e.getValue().characterId!=368);
  if(NativePages.pf.layers.size()!=1)throw new IOException("Missing close symbol");
  NativePages.pf.layers.values().iterator().next().isVisible=true;
  BufferedImage image=SWF.frameToImageGet(NativePages.root,700,0,null,0,NativePages.s.displayRect,new Matrix(),null,new Color(0,0,0,0),1.0,true,3).getBufferedImage();
  ImageIO.write(image,"png",new File(args[1]));
 }
}
