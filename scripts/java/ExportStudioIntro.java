import com.jpexs.decompiler.flash.SWF;
import com.jpexs.decompiler.flash.tags.DefineSpriteTag;
import com.jpexs.decompiler.flash.timeline.Timeline;
import com.jpexs.decompiler.flash.exporters.commonshape.Matrix;
import java.io.*;
import java.awt.*;
import java.awt.image.BufferedImage;
import java.util.zip.Deflater;
import javax.imageio.ImageIO;
/* Original State of Play opening, isolated for the lower 3DS screen. */
public class ExportStudioIntro {
 // Fixed authored offset centers the complete sign and website without frame-to-frame jitter.
 static final int OFFSET_X=-24, OFFSET_Y=-20;
 static void le(RandomAccessFile f,int n)throws Exception{f.writeInt(Integer.reverseBytes(n));}
 static void sh(RandomAccessFile f,int n)throws Exception{f.writeShort(Short.reverseBytes((short)n));}
 public static void main(String[]args)throws Exception{
  SWF swf=new SWF(new FileInputStream(args[0]),false);
  Timeline timeline=((DefineSpriteTag)swf.getCharacter(48)).getTimeline();int count=timeline.getFrames().size();
  new File("project/romfs").mkdirs();new File("qa").mkdirs();
  try(RandomAccessFile f=new RandomAccessFile("project/romfs/studio_intro.lan","rw")){
   f.setLength(0);f.writeBytes("LAN2");le(f,320);le(f,240);le(f,25);le(f,count);
   long index=f.getFilePointer();for(int i=0;i<=count;i++)le(f,0);int[]offsets=new int[count+1];
   for(int frame=0;frame<count;frame++){
    BufferedImage original=SWF.frameToImageGet(timeline,frame,0,null,0,swf.displayRect,new Matrix(),null,Color.BLACK,1.0,true,3).getBufferedImage();
    BufferedImage canvas=new BufferedImage(320,240,BufferedImage.TYPE_INT_ARGB);Graphics2D g=canvas.createGraphics();
    g.setColor(Color.BLACK);g.fillRect(0,0,320,240);g.setRenderingHint(RenderingHints.KEY_INTERPOLATION,RenderingHints.VALUE_INTERPOLATION_BICUBIC);g.setClip(0,13,320,213);g.drawImage(original,OFFSET_X,13+OFFSET_Y,320,213,null);g.dispose();
    byte[]raw=new byte[320*240*4];for(int y=0;y<240;y++)for(int x=0;x<320;x++){int v=canvas.getRGB(x,y),at=(y*320+x)*4;raw[at]=(byte)(v>>16);raw[at+1]=(byte)(v>>8);raw[at+2]=(byte)v;raw[at+3]=(byte)(v>>24);}
    Deflater z=new Deflater(6);z.setInput(raw);z.finish();byte[]packed=new byte[raw.length+1024];int n=z.deflate(packed);if(!z.finished())throw new IOException("Compression capacity");z.end();
    offsets[frame]=(int)f.getFilePointer();le(f,4<<16);sh(f,0);sh(f,0);sh(f,320);sh(f,240);f.write(packed,0,n);
    if(frame%40==0)ImageIO.write(canvas,"png",new File("qa/studio-"+frame+".png"));
   }
   offsets[count]=(int)f.getFilePointer();f.seek(index);for(int n:offsets)le(f,n);
  }
  System.out.println("Studio source frames: "+count);
 }
}
