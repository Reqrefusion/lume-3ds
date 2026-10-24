import com.jpexs.decompiler.flash.SWF;
import com.jpexs.decompiler.flash.tags.DefineSpriteTag;
import com.jpexs.decompiler.flash.timeline.*;
import com.jpexs.decompiler.flash.exporters.commonshape.Matrix;
import java.io.*;import java.awt.Color;import java.awt.image.BufferedImage;import java.util.*;import java.util.zip.Deflater;import javax.imageio.ImageIO;
/* Render authored timelines offline. A terminal stop is a hold, never a modulo loop.
   The particle sprite has no stop and keeps advancing when its parent stops. */
public class ExportTitleMotion {
 static SWF s;static Timeline outer,menu,anim;static Map<Integer,DepthState> layers;static DepthState menuAnim;
 static Timeline timeline(int id){return ((DefineSpriteTag)s.getCharacter(id)).getTimeline();}
 static BufferedImage image(Color bg){return SWF.frameToImageGet(outer,10,0,null,0,s.displayRect,new Matrix(),null,bg,1.0,true,3).getBufferedImage();}
 static void le(RandomAccessFile f,int v)throws Exception{f.writeInt(Integer.reverseBytes(v));}
 static void sh(RandomAccessFile f,int v)throws Exception{f.writeShort(Short.reverseBytes((short)v));}
 static byte[] rgba(BufferedImage im){byte[] b=new byte[800*600*4];for(int y=0;y<533;y++)for(int x=0;x<800;x++){int p=im.getRGB(x,y),i=((y+33)*800+x)*4;b[i]=(byte)(p>>16);b[i+1]=(byte)(p>>8);b[i+2]=(byte)p;b[i+3]=(byte)(p>>24);}return b;}
 static void position(int phase,int k){
  int frame=phase==0?Math.min(k,732):phase==1?732:733+k;
  menuAnim.time=frame;
  Frame f=anim.getFrame(frame);
  int extra=phase==1?4+k:phase==0?Math.max(0,k-732):0;
  for(DepthState d:f.layers.values()){
   if(d.characterId==80)d.time=(frame==732?496+extra:d.time);
   if(d.characterId==127)d.time=Math.min(frame==732?67+extra:d.time,70);
   if(d.characterId==119)d.time=Math.min(d.time,334);
  }
 }
 static void export(int phase,int count,String name)throws Exception{
  try(RandomAccessFile f=new RandomAccessFile("project/romfs/title_"+name+".lan","rw")){
   f.setLength(0);f.writeBytes("LAN2");le(f,800);le(f,600);le(f,25);le(f,count);
   long index=f.getFilePointer();for(int k=0;k<=count;k++)le(f,0);
   int[] offsets=new int[count+1];byte[] compressed=new byte[800*600*4+1024];
   for(int k=0;k<count;k++){
    position(phase,k);BufferedImage im=image(null);byte[] b=rgba(im);
    // Black bars are not part of the source viewport.
    Deflater z=new Deflater(6);z.setInput(b);z.finish();int n=z.deflate(compressed);if(!z.finished())throw new IOException("deflate capacity");z.end();
    offsets[k]=(int)f.getFilePointer();le(f,4<<16);sh(f,0);sh(f,0);sh(f,800);sh(f,600);f.write(compressed,0,n);
    if(k==0||k==count-1||phase==0&&(k==150||k==400||k==650))ImageIO.write(im,"png",new File("qa/title-"+name+"-"+k+".png"));
    if(k%50==0)System.out.println(name+" "+k+"/"+count);
   }
   offsets[count]=(int)f.getFilePointer();f.seek(index);for(int x:offsets)le(f,x);
  }
 }
 public static void main(String[]a)throws Exception{
  s=new SWF(new FileInputStream(a[0]),false);outer=timeline(1038);menu=timeline(170);anim=timeline(128);layers=new TreeMap<>(menu.getFrame(0).layers);
  for(DepthState d:outer.getFrame(10).layers.values()){if(d.characterId==170)d.time=0;if(d.characterId==173)d.time=0;}
  for(DepthState d:layers.values()){if("menuAnim".equals(d.instanceName))menuAnim=d;if("btnContinue".equals(d.instanceName))d.isVisible=false;if("confirmBox".equals(d.instanceName)||"creditsBox".equals(d.instanceName))d.time=0;}
  // Repair independent stopped child clocks on every authored parent frame.
  for(Frame f:anim.getFrames())for(DepthState d:f.layers.values()){if(d.characterId==119)d.time=Math.min(d.time,334);if(d.characterId==127)d.time=Math.min(d.time,70);}
  // Buttons live below menuAnim: preserve that order so entry/exit covers
  // also cover Continue, rather than painting it over a black transition.
  menuAnim.isVisible=false;
  BufferedImage base=image(Color.BLACK);byte[] rgb=new byte[800*600*3];
  for(int y=0;y<533;y++)for(int x=0;x<800;x++){int v=base.getRGB(x,y),i=((y+33)*800+x)*3;rgb[i]=(byte)(v>>16);rgb[i+1]=(byte)(v>>8);rgb[i+2]=(byte)v;}
  java.nio.file.Files.write(java.nio.file.Path.of("project/romfs/title_motion_base.rgb"),rgb);
  menuAnim.isVisible=true;
  menu.getFrame(0).layers.entrySet().removeIf(e->e.getValue()!=menuAnim);
  export(0,736,"entry");export(1,365,"idle");export(2,43,"exit");
  // Popup and Continue layers remain independent of the moving background.
  for(String which:new String[]{"continue","credits","confirm"}){
   menu.getFrame(0).layers.clear();for(var e:layers.entrySet())if(e.getValue().characterId==(which.equals("continue")?56:which.equals("credits")?169:142)){DepthState d=e.getValue();d.isVisible=true;d.time=which.equals("continue")?0:9;menu.getFrame(0).layers.put(e.getKey(),d);}
   for(DepthState d:outer.getFrame(10).layers.values())if(d.characterId!=170)d.isVisible=false;
   java.nio.file.Files.write(java.nio.file.Path.of("project/romfs/title_"+which+".rgba"),rgba(image(null)));
   ImageIO.write(image(null),"png",new File("qa/title-layer-"+which+".png"));
  }
  ExportTitleExit.main(a);
 }
}
