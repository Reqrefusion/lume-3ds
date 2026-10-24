import com.jpexs.decompiler.flash.SWF;
import com.jpexs.decompiler.flash.tags.DefineSpriteTag;
import com.jpexs.decompiler.flash.timeline.*;
import com.jpexs.decompiler.flash.exporters.commonshape.Matrix;
import java.io.*;import java.nio.file.*;import java.awt.image.BufferedImage;
/* Only the authored door's final over state is made persistent. Mouse-follow
   walk cursors and selected menu states are not independent scene artwork. */
public class HoverArt {
 public static void main(String[]args)throws Exception {
  Path out=Path.of("project/romfs/hover.bin");Files.createDirectories(out.getParent());
  try(DataOutputStream f=new DataOutputStream(new BufferedOutputStream(Files.newOutputStream(out)))){
   f.writeBytes("LHV1");f.writeInt(Integer.reverseBytes(2));
   for(int frame:new int[]{701,970}){
    SWF swf=new SWF(new FileInputStream(args[0]),false);
    Timeline root=((DefineSpriteTag)swf.getCharacter(988)).getTimeline();
    Frame camera=root.getFrame(frame-1);camera.layers.entrySet().removeIf(e->!"you".equals(e.getValue().instanceName));
    DepthState you=camera.layers.values().iterator().next();you.time=0;
    Timeline actor=((DefineSpriteTag)swf.getCharacter(you.characterId)).getTimeline();
    for(Frame a:actor.getFrames())a.layers.entrySet().removeIf(e->!"hotspot_door".equals(e.getValue().instanceName));
    for(Frame a:actor.getFrames())for(DepthState d:a.layers.values()){
     Timeline door=((DefineSpriteTag)swf.getCharacter(d.characterId)).getTimeline();
     int last=door.getFrameCount()-1;d.time=last;
    }
    BufferedImage im=SWF.frameToImageGet(root,frame-1,0,null,0,swf.displayRect,new Matrix(),null,null,1.0,true,3).getBufferedImage();
    int x0=800,y0=533,x1=0,y1=0;
    for(int y=0;y<im.getHeight();y++)for(int x=0;x<im.getWidth();x++)if((im.getRGB(x,y)>>>24)>0){x0=Math.min(x0,x);y0=Math.min(y0,y);x1=Math.max(x1,x+1);y1=Math.max(y1,y+1);}
    if(x1<=x0||y1<=y0)throw new IOException("Missing original exit arrow "+frame);
    for(int v:new int[]{frame==701?3:4,x0,y0+33,x1-x0,y1-y0})f.writeInt(Integer.reverseBytes(v));
    for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++){int p=im.getRGB(x,y);f.writeByte(p>>16);f.writeByte(p>>8);f.writeByte(p);f.writeByte(p>>>24);}
    Files.createDirectories(Path.of("qa"));javax.imageio.ImageIO.write(im,"png",new File("qa/exit_"+frame+".png"));
    System.out.println("Original exit scene "+frame+": "+x0+","+(y0+33)+" "+(x1-x0)+"x"+(y1-y0));
   }
  }
 }
}
