import java.io.*;import java.awt.Color;import java.awt.image.BufferedImage;import java.util.*;import java.util.zip.Deflater;
import com.jpexs.decompiler.flash.SWF;import com.jpexs.decompiler.flash.timeline.*;import com.jpexs.decompiler.flash.exporters.commonshape.Matrix;
class ExportLockProgress {
 public static void main(String[]a)throws Exception{
  ExportPuzzleFeedback.s=new SWF(new FileInputStream(a[0]),false);Timeline root=ExportPuzzleFeedback.timeline(988);Frame rf=root.getFrame(700);rf.layers.entrySet().removeIf(e->!"popups".equals(e.getValue().instanceName));rf.layers.values().iterator().next().time=59;
  Timeline pop=ExportPuzzleFeedback.timeline(753);for(DepthState d:pop.getFrame(59).layers.values())if("resetTimerAnim".equals(d.instanceName))d.time=0;
  Timeline lock=ExportPuzzleFeedback.timeline(553);DepthState target=null;for(DepthState d:lock.getFrame(0).layers.values())if("lock".equals(d.instanceName))target=d;if(target==null)throw new IOException("lock");target.time=0;
  BufferedImage base=SWF.frameToImageGet(root,700,0,null,0,ExportPuzzleFeedback.s.displayRect,new Matrix(),null,Color.BLACK,1.0,true,3).getBufferedImage();
  javax.imageio.ImageIO.write(base,"png",new File("qa/feedback-lock-base.png"));
  java.nio.file.Path out=java.nio.file.Path.of("project/romfs/ui_cupboard_progress.lan"),temp=java.nio.file.Path.of(out+".tmp");
  try(RandomAccessFile f=new RandomAccessFile(temp.toFile(),"rw")) {
   f.setLength(0);f.writeBytes("LAN2");ExportPuzzleFeedback.le(f,800);ExportPuzzleFeedback.le(f,600);ExportPuzzleFeedback.le(f,25);ExportPuzzleFeedback.le(f,3);for(int i=0;i<4;i++)ExportPuzzleFeedback.le(f,0);int[] offsets=new int[4];
   for(int stage=1;stage<=3;stage++){
    target.time=stage;BufferedImage im=SWF.frameToImageGet(root,700,0,null,0,ExportPuzzleFeedback.s.displayRect,new Matrix(),null,Color.BLACK,1.0,true,3).getBufferedImage();byte[] rgba=new byte[800*600*4];
    for(int y=0;y<533;y++)for(int x=0;x<800;x++){int v=im.getRGB(x,y),i=((y+33)*800+x)*4;if(v!=base.getRGB(x,y)){rgba[i]=(byte)(v>>16);rgba[i+1]=(byte)(v>>8);rgba[i+2]=(byte)v;rgba[i+3]=(byte)255;}}
    offsets[stage-1]=(int)f.getFilePointer();ExportPuzzleFeedback.le(f,4<<16);ExportPuzzleFeedback.sh(f,0);ExportPuzzleFeedback.sh(f,0);ExportPuzzleFeedback.sh(f,800);ExportPuzzleFeedback.sh(f,600);Deflater z=new Deflater(6);z.setInput(rgba);z.finish();byte[] b=new byte[rgba.length+2048];int n=z.deflate(b);if(!z.finished())throw new IOException("deflate");z.end();f.write(b,0,n);
    javax.imageio.ImageIO.write(im,"png",new File("qa/feedback-lock-stage-"+stage+".png"));
   }offsets[3]=(int)f.getFilePointer();f.seek(20);for(int n:offsets)ExportPuzzleFeedback.le(f,n);
  }
  java.nio.file.Files.move(temp,out,java.nio.file.StandardCopyOption.REPLACE_EXISTING,java.nio.file.StandardCopyOption.ATOMIC_MOVE);
 }
}
