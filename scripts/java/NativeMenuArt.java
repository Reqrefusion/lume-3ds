import com.jpexs.decompiler.flash.SWF;
import com.jpexs.decompiler.flash.tags.DefineSpriteTag;
import com.jpexs.decompiler.flash.tags.base.StaticTextTag;
import com.jpexs.decompiler.flash.timeline.*;
import com.jpexs.decompiler.flash.timeline.Frame;
import com.jpexs.decompiler.flash.exporters.commonshape.Matrix;
import com.jpexs.decompiler.flash.types.RECT;
import java.awt.*;import java.awt.image.BufferedImage;import java.io.*;import java.nio.file.*;
/* Source inventoryItems=1017 and btnSave/btnQuit=1022/1028 were inspected
   directly in the verified original SWF. Render source paths, text and fills
   before resizing; never crop the old 320px toolbar or invent button artwork. */
public class NativeMenuArt {
 static final int[] IDS={1004,1007,1009,1010,1013,1014,1016,1029,1022,1022,1028};
 static final int[] WIDTH={64,64,64,64,64,64,64,200,216,216,216};
 static final int[] HEIGHT={64,64,64,64,64,64,64,28,80,80,80};
 static BufferedImage source(SWF swf,int id,int frame,boolean title)throws Exception {
  DefineSpriteTag tag=(DefineSpriteTag)swf.getCharacter(id);Timeline timeline=tag.getTimeline();
  if(title)for(DepthState layer:timeline.getFrame(0).layers.values())layer.isVisible=layer.characterId==995;
  RECT r=tag.getRect();Matrix matrix=new Matrix();matrix.scaleX=matrix.scaleY=4;
  matrix.translateX=-r.Xmin*4+80;matrix.translateY=-r.Ymin*4+80;
  BufferedImage image=SWF.frameToImageGet(timeline,frame,0,null,0,
   new RECT(0,(r.Xmax-r.Xmin)*4+160,0,(r.Ymax-r.Ymin)*4+160),matrix,null,null,1.0,true,3).getBufferedImage();
  int x0=image.getWidth(),y0=image.getHeight(),x1=0,y1=0;
  for(int y=0;y<image.getHeight();y++)for(int x=0;x<image.getWidth();x++)if((image.getRGB(x,y)>>>24)>0){x0=Math.min(x0,x);y0=Math.min(y0,y);x1=Math.max(x1,x+1);y1=Math.max(y1,y+1);}
  if(x1<=x0||y1<=y0)throw new IOException("Missing original menu art: "+id+" frame "+frame);
  return image.getSubimage(x0,y0,x1-x0,y1-y0);
 }
 public static void main(String[] args)throws Exception {
  SWF swf=new SWF(new FileInputStream(args[0]),false);Path out=Path.of("project/romfs/native_menu");Files.createDirectories(out);
  try(DataOutputStream file=new DataOutputStream(new BufferedOutputStream(Files.newOutputStream(out.resolve("art.bin"))))){
   file.writeBytes("LNA1");file.writeInt(Integer.reverseBytes(IDS.length));
   for(int i=0;i<IDS.length;i++){file.writeShort(Short.reverseBytes((short)WIDTH[i]));file.writeShort(Short.reverseBytes((short)HEIGHT[i]));}
   for(int i=0;i<IDS.length;i++){
    BufferedImage source=source(swf,IDS[i],i==9?10:0,i==7);
    BufferedImage target=new BufferedImage(WIDTH[i],HEIGHT[i],BufferedImage.TYPE_INT_ARGB);Graphics2D g=target.createGraphics();
    g.setRenderingHint(RenderingHints.KEY_INTERPOLATION,RenderingHints.VALUE_INTERPOLATION_BICUBIC);
    double scale=Math.min((double)WIDTH[i]/source.getWidth(),(double)HEIGHT[i]/source.getHeight());
    int w=(int)Math.round(source.getWidth()*scale),h=(int)Math.round(source.getHeight()*scale);
    g.drawImage(source,(WIDTH[i]-w)/2,(HEIGHT[i]-h)/2,w,h,null);g.dispose();
    for(int y=0;y<HEIGHT[i];y++)for(int x=0;x<WIDTH[i];x++){
     int pixel=target.getRGB(x,y),alpha=pixel>>>24;
     file.writeByte(((pixel>>>16&255)*alpha+127)/255);file.writeByte(((pixel>>>8&255)*alpha+127)/255);file.writeByte(((pixel&255)*alpha+127)/255);file.writeByte(alpha);
    }
    System.out.println("Original menu sprite "+IDS[i]+" frame "+(i==9?10:0)+": "+source.getWidth()+"x"+source.getHeight()+" -> "+WIDTH[i]+"x"+HEIGHT[i]);
   }
  }
  StaticTextTag save=(StaticTextTag)swf.getCharacter(1019);
  int font=save.textRecords.stream().filter(r->r.styleFlagsHasFont).findFirst().orElseThrow().fontId;
  ReadingText.fontAtlas(swf,swf.getFont(font),out.resolve("font.bin"));
  System.out.println("Menu art/font ready; no video conversion.");
 }
}
