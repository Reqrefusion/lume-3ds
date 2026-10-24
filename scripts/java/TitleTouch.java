import com.jpexs.decompiler.flash.SWF;
import com.jpexs.decompiler.flash.tags.DefineSpriteTag;
import com.jpexs.decompiler.flash.timeline.*;
import com.jpexs.decompiler.flash.timeline.Frame;
import com.jpexs.decompiler.flash.exporters.commonshape.Matrix;
import com.jpexs.decompiler.flash.types.RECT;
import java.awt.*;import java.awt.image.BufferedImage;import java.io.*;import java.nio.file.*;import java.util.*;import java.util.zip.Deflater;
/* Adapt only the presentation of the authored menu. The original animation,
   sound cue and save-state timelines remain independent of bottom controls. */
class TitleTouch {
 static String path;static SWF s;static Timeline outer,menu,anim;static DepthState motion;
 static Timeline t(int id){return ((DefineSpriteTag)s.getCharacter(id)).getTimeline();}
 static void setup()throws Exception{
  s=new SWF(new FileInputStream(path),false);outer=t(1038);menu=t(170);anim=t(128);
  for(DepthState d:outer.getFrame(10).layers.values()){if(d.characterId==170)d.time=0;else d.isVisible=false;}
  for(DepthState d:menu.getFrame(0).layers.values()){d.isVisible=false;if("menuAnim".equals(d.instanceName))motion=d;}
 }
 static BufferedImage full()throws Exception{return SWF.frameToImageGet(outer,10,0,null,0,s.displayRect,new Matrix(),null,Color.BLACK,1.0,true,3).getBufferedImage();}
 static Rectangle bounds(BufferedImage im){int x0=im.getWidth(),y0=im.getHeight(),x1=0,y1=0;for(int y=0;y<im.getHeight();y++)for(int x=0;x<im.getWidth();x++)if((im.getRGB(x,y)>>>24)>0){x0=Math.min(x0,x);y0=Math.min(y0,y);x1=Math.max(x1,x+1);y1=Math.max(y1,y+1);}return new Rectangle(x0,y0,x1-x0,y1-y0);}
 static BufferedImage local(int id,int frame)throws Exception{
  DefineSpriteTag tag=(DefineSpriteTag)s.getCharacter(id);RECT r=tag.getRect();Matrix m=new Matrix();m.scaleX=m.scaleY=4;m.translateX=-r.Xmin*4+80;m.translateY=-r.Ymin*4+80;
  BufferedImage im=SWF.frameToImageGet(tag.getTimeline(),frame,0,null,0,new RECT(0,(r.Xmax-r.Xmin)*4+160,0,(r.Ymax-r.Ymin)*4+160),m,null,null,1.0,true,3).getBufferedImage();Rectangle b=bounds(im);return im.getSubimage(b.x,b.y,b.width,b.height);
 }
 static BufferedImage background;
 static BufferedImage page(){BufferedImage out=new BufferedImage(800,600,BufferedImage.TYPE_INT_RGB);Graphics2D g=out.createGraphics();g.setRenderingHint(RenderingHints.KEY_INTERPOLATION,RenderingHints.VALUE_INTERPOLATION_BICUBIC);g.drawImage(background,-50,0,900,600,null);g.setColor(new Color(0,0,0,145));g.fillRect(0,0,800,600);g.dispose();return out;}
 static void fit(Graphics2D g,BufferedImage art,int x,int y,int w,int h){double z=Math.min((double)w/art.getWidth(),(double)h/art.getHeight());int nw=(int)Math.round(art.getWidth()*z),nh=(int)Math.round(art.getHeight()*z);g.drawImage(art,x+(w-nw)/2,y+(h-nh)/2,nw,nh,null);}
 static void save(String name,BufferedImage im)throws Exception{byte[] raw=new byte[800*600*3];for(int y=0;y<600;y++)for(int x=0;x<800;x++){int v=im.getRGB(x,y),i=(y*800+x)*3;raw[i]=(byte)(v>>16);raw[i+1]=(byte)(v>>8);raw[i+2]=(byte)v;}Files.write(Path.of("project/romfs/"+name+".rgb"),raw);javax.imageio.ImageIO.write(im,"png",new File("qa/"+name+".png"));}
 static int[][] rects=new int[9][4];
 static void modal(String which,int id,int selected)throws Exception{
  setup();DepthState box=null;for(DepthState d:menu.getFrame(0).layers.values())if(d.characterId==id){d.isVisible=true;d.time=9;box=d;}if(box==null)throw new IOException("Modal missing");
  BufferedImage art=SWF.frameToImageGet(outer,10,0,null,0,s.displayRect,new Matrix(),null,null,1.0,true,3).getBufferedImage();Rectangle b=bounds(art);art=art.getSubimage(b.x,b.y,b.width,b.height);
  BufferedImage out=page();Graphics2D g=out.createGraphics();g.setRenderingHint(RenderingHints.KEY_INTERPOLATION,RenderingHints.VALUE_INTERPOLATION_BICUBIC);
  int maxW=which.equals("credits")?720:736,maxH=which.equals("credits")?510:320;double z=Math.min((double)maxW/b.width,(double)maxH/b.height);int w=(int)Math.round(b.width*z),h=(int)Math.round(b.height*z),x=(800-w)/2,y=which.equals("credits")?12:135;
  g.drawImage(art,x,y,w,h,null);g.dispose();
  int[][] old=which.equals("credits")?new int[][]{{140,160,38,17}}:new int[][]{{113,140,37,20},{164,140,37,20}};
  for(int i=0;i<old.length;i++){int[]o=old[i];int slot=which.equals("credits")?7:5+i;rects[slot]=new int[]{(int)Math.floor((x+(o[0]*2.5-b.x)*z)/2.5),(int)Math.floor((y+((o[1]-13)*2.5-b.y)*z)/2.5),(int)Math.ceil(o[2]*z),(int)Math.ceil(o[3]*z)};}
  if(!which.equals("credits")){int[]q=rects[5+selected];Graphics2D hi=out.createGraphics();hi.setColor(new Color(255,160,65));hi.setStroke(new BasicStroke(3));hi.drawRect(q[0]*5/2,q[1]*5/2,q[2]*5/2,q[3]*5/2);hi.dispose();}
  save("title_touch_"+which+(which.equals("credits")?"":"_"+selected),out);
 }
 static void controls()throws Exception{
  setup();for(DepthState d:menu.getFrame(0).layers.values())if(d.characterId==50)d.isVisible=true;background=full();BufferedImage top=new BufferedImage(800,600,BufferedImage.TYPE_INT_RGB);Graphics2D tg=top.createGraphics();tg.drawImage(background,0,33,null);tg.dispose();save("title_touch_top",top);
  BufferedImage[] glyphs={local(56,0),local(54,0),local(58,0)};
  for(int saved=0;saved<2;saved++)for(int selection=saved==1?0:1;selection<3;selection++){
   BufferedImage out=page();Graphics2D g=out.createGraphics();g.setRenderingHint(RenderingHints.KEY_INTERPOLATION,RenderingHints.VALUE_INTERPOLATION_BICUBIC);
   for(int item=saved==1?0:1;item<3;item++){int y=saved==1?38+56*item:76+60*(item-1);int slot=saved==1?item:2+item;rects[slot]=new int[]{24,y,272,44};BufferedImage art=glyphs[item];BufferedImage ink=new BufferedImage(art.getWidth(),art.getHeight(),BufferedImage.TYPE_INT_ARGB);for(int yy=0;yy<art.getHeight();yy++)for(int xx=0;xx<art.getWidth();xx++){int p=art.getRGB(xx,yy),a=p>>>24;if(a!=0)ink.setRGB(xx,yy,a<<24|(item==selection?0xf79424:0x00cda5));}fit(g,ink,210,y*5/2+12,380,86);}
   g.dispose();save("title_touch_"+(saved==1?"saved":"empty")+"_"+selection,out);
  }
  modal("credits",169,0);modal("confirm",142,0);modal("confirm",142,1);
  setup();rects[8]=new int[]{8,200,40,40};
  try(ExportPuzzleArt.Lan sound=new ExportPuzzleArt.Lan("title_touch_sound",2)){
   for(int k=0;k<2;k++){BufferedImage im=new BufferedImage(800,600,BufferedImage.TYPE_INT_ARGB);Graphics2D g=im.createGraphics();fit(g,local(173,k==0?0:4),30,520,60,60);g.dispose();sound.add(im,null);}
  }
  try(DataOutputStream f=new DataOutputStream(new FileOutputStream("project/romfs/title_touch.layout"))){f.writeBytes("LTT1");for(int[]r:rects)for(int v:r)f.writeShort(Short.reverseBytes((short)v));}
 }
 static void position(int phase,int k){int frame=phase==0?Math.min(k,732):phase==1?732:733+k;motion.time=frame;Frame f=anim.getFrame(frame);int extra=phase==1?4+k:phase==0?Math.max(0,k-732):0;for(DepthState d:f.layers.values()){if(d.characterId==80)d.time=frame==732?496+extra:d.time;if(d.characterId==127)d.time=Math.min(frame==732?67+extra:d.time,70);if(d.characterId==119)d.time=Math.min(d.time,334);}}
 static void corner(int phase,int count,String name)throws Exception{
  setup();motion.isVisible=true;
  if(phase==2)for(int i=733;i<776;i++)anim.getFrame(i).layers.entrySet().removeIf(e->e.getValue().characterId!=64&&e.getValue().characterId!=61);
  for(Frame f:anim.getFrames())for(DepthState d:f.layers.values()){if(d.characterId==119)d.time=Math.min(d.time,334);if(d.characterId==127)d.time=Math.min(d.time,70);}
  try(RandomAccessFile f=new RandomAccessFile("project/romfs/title_touch_"+name+".lta","rw")){
   f.setLength(0);f.writeBytes("LAN2");ExportTitleMotion.le(f,320);ExportTitleMotion.le(f,240);ExportTitleMotion.le(f,25);ExportTitleMotion.le(f,count);int[] ix=new int[count+1];for(int k=0;k<=count;k++)ExportTitleMotion.le(f,0);
   for(int k=0;k<count;k++){
    position(phase,k);BufferedImage im=SWF.frameToImageGet(outer,10,0,null,0,new RECT(0,1000,0,1000),new Matrix(),null,null,.4,true,3).getBufferedImage();
    byte[] raw=new byte[20*20*4];for(int y=0;y<20;y++)for(int x=0;x<20;x++){int p=im.getRGB(x,y),i=(y*20+x)*4;raw[i]=(byte)(p>>16);raw[i+1]=(byte)(p>>8);raw[i+2]=(byte)p;raw[i+3]=(byte)(p>>>24);}
    Deflater z=new Deflater(6);z.setInput(raw);z.finish();byte[] b=new byte[2048];int n=z.deflate(b);if(!z.finished())throw new IOException("corner deflate");z.end();ix[k]=(int)f.getFilePointer();ExportTitleMotion.le(f,4<<16);ExportTitleMotion.sh(f,0);ExportTitleMotion.sh(f,13);ExportTitleMotion.sh(f,20);ExportTitleMotion.sh(f,20);f.write(b,0,n);
   }
   ix[count]=(int)f.getFilePointer();f.seek(20);for(int v:ix)ExportTitleMotion.le(f,v);
  }System.out.println("title corner "+name+" "+count);
 }
 public static void main(String[]a)throws Exception{path=a[0];controls();corner(0,736,"entry");corner(1,365,"idle");corner(2,43,"exit");}
}
