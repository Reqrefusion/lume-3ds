import com.jpexs.decompiler.flash.SWF;import com.jpexs.decompiler.flash.tags.DefineSpriteTag;import com.jpexs.decompiler.flash.timeline.*;import com.jpexs.decompiler.flash.exporters.commonshape.Matrix;
import java.io.*;import java.util.*;import java.awt.image.BufferedImage;import java.util.zip.Deflater;import javax.imageio.ImageIO;
/* Source popup matrices are preserved. Export only feedback, never the puzzle
   underneath: native board state must remain visible while its result plays. */
class ExportPuzzleFeedback{
 static SWF s;static Timeline root;static DepthState target;static String swfPath;
 static Timeline timeline(int id){return ((DefineSpriteTag)s.getCharacter(id)).getTimeline();}
 static boolean isolate(Timeline t,int frame,String name,int depth){
  if(depth>5)return false;Frame f=t.getFrame(frame%t.getFrameCount());
  for(var e:new ArrayList<>(f.layers.entrySet())){DepthState d=e.getValue();
   if(name.equals(d.instanceName)){target=d;f.layers.entrySet().removeIf(x->x.getKey().intValue()!=e.getKey().intValue());return true;}
   if(s.getCharacter(d.characterId) instanceof DefineSpriteTag && isolate(timeline(d.characterId),d.time,name,depth+1)){f.layers.entrySet().removeIf(x->x.getKey().intValue()!=e.getKey().intValue());return true;}
  }return false;
 }
 static void le(RandomAccessFile f,int n)throws Exception{f.writeInt(Integer.reverseBytes(n));}
 static void sh(RandomAccessFile f,int n)throws Exception{f.writeShort(Short.reverseBytes((short)n));}
 static void export(String name,int popup,String targetName,int first,int count)throws Exception{
  s=new SWF(new FileInputStream(swfPath),false);root=timeline(988);
  for(Frame df:timeline(310).getFrames())for(DepthState d:df.layers.values())
    if(d.characterId==232)d.time=Math.min(d.time,1);
  Frame rf=root.getFrame(700);rf.layers.entrySet().removeIf(e->!"popups".equals(e.getValue().instanceName));DepthState p=rf.layers.values().iterator().next();p.time=popup-1;
  if(name.equals("power_fade")) {
   s=new SWF(new FileInputStream(swfPath),false);root=timeline(988);Frame scene=root.getFrame(700);scene.layers.entrySet().removeIf(e->!"fadeToBlack".equals(e.getValue().instanceName));target=scene.layers.values().iterator().next();
  } else if(!isolate(timeline(753),popup-1,targetName,0))throw new IOException("Missing "+targetName);
  /* Solar contains full dialogue, which belongs to the translated reader.
     Short source result labels, marks and their entrance remain intact. */
  if(name.equals("solar_win"))
   for(var tag:s.getCharacters(false).values())if(tag instanceof DefineSpriteTag sp)
    for(Frame frame:sp.getTimeline().getFrames())frame.layers.entrySet().removeIf(e->s.getCharacter(e.getValue().characterId) instanceof com.jpexs.decompiler.flash.tags.base.TextTag);
  java.nio.file.Path out=java.nio.file.Path.of("project/romfs/ui_"+name+".lan"),temp=java.nio.file.Path.of(out+".tmp");
  try(RandomAccessFile f=new RandomAccessFile(temp.toFile(),"rw")){
   f.setLength(0);f.writeBytes("LAN2");le(f,800);le(f,600);le(f,25);le(f,count);long idx=f.getFilePointer();for(int k=0;k<=count;k++)le(f,0);int[] offsets=new int[count+1];
   for(int k=0;k<count;k++){
    target.time=first-1+k;
    if(name.equals("wires_incorrect")&&k==count-1)target.time=0;
    if(name.equals("solar_win")){
     target.time=Math.min(first-1+k,747);
     for(DepthState child:timeline(310).getFrame(target.time).layers.values())if(child.characterId==309)child.time=k;
    }
    BufferedImage im=SWF.frameToImageGet(root,700,0,null,0,s.displayRect,new Matrix(),null,null,1.0,true,3).getBufferedImage();
    int x0=800,y0=533,x1=0,y1=0;for(int y=0;y<533;y++)for(int x=0;x<800;x++)if((im.getRGB(x,y)>>>24)!=0){x0=Math.min(x0,x);x1=Math.max(x1,x+1);y0=Math.min(y0,y);y1=Math.max(y1,y+1);}
    offsets[k]=(int)f.getFilePointer();le(f,4<<16);
    if(x1<=x0||y1<=y0){sh(f,0);sh(f,0);sh(f,0);sh(f,0);}
    else {int w=x1-x0,h=y1-y0;sh(f,x0);sh(f,y0+33);sh(f,w);sh(f,h);byte[] raw=new byte[w*h*4];for(int y=0;y<h;y++)for(int x=0;x<w;x++){int v=im.getRGB(x+x0,y+y0),i=(y*w+x)*4;raw[i]=(byte)(v>>16);raw[i+1]=(byte)(v>>8);raw[i+2]=(byte)v;raw[i+3]=(byte)(v>>>24);}Deflater z=new Deflater(6);z.setInput(raw);z.finish();byte[] b=new byte[raw.length+2048];int n=z.deflate(b);if(!z.finished())throw new IOException("deflate");z.end();f.write(b,0,n);}
    if(k==Math.min(11,count-1)||k==count-1)ImageIO.write(im,"png",new File("qa/feedback-"+name+"-"+k+".png"));
   }
   offsets[count]=(int)f.getFilePointer();f.seek(idx);for(int n:offsets)le(f,n);
  }
  java.nio.file.Files.move(temp,out,java.nio.file.StandardCopyOption.REPLACE_EXISTING,java.nio.file.StandardCopyOption.ATOMIC_MOVE);
  System.out.println(name+" "+count);
 }
 public static void main(String[]a)throws Exception{
  swfPath=a[0];
  boolean results=a.length>1&&a[1].equals("--results");
  if(!results){
  export("keys_seen",41,"dialogueZoomed",681,12);export("keys_incorrect",41,"dialogueZoomed",669,12);
  export("map_seen",80,"dialogueZoomed",327,12);export("map_incorrect",80,"dialogueZoomed",339,12);
  }
  export("solar_win",31,"dialogueZoomed",728,115);
  export("wires_correct",50,"messageBox",42,58);export("wires_incorrect",50,"messageBox",11,32);export("map_correct",80,"messageBox",11,45);
  export("result_timer",41,"resetTimerAnim",11,52);
  export("power_fade",31,"fadeToBlack",13,277);
  if(results)return;
  // Solar's solve handler hides its board before showing its completion line.
  s=new SWF(new FileInputStream(swfPath),false);root=timeline(988);Frame rf=root.getFrame(700);rf.layers.entrySet().removeIf(e->!"popups".equals(e.getValue().instanceName));rf.layers.values().iterator().next().time=30;
  for(DepthState d:timeline(753).getFrame(30).layers.values()) {
    if("solarPanel".equals(d.instanceName))d.isVisible=false;
    if("dialogueZoomed".equals(d.instanceName)||"resetTimerAnim".equals(d.instanceName))d.time=0;
  }
  BufferedImage plate=SWF.frameToImageGet(root,700,0,null,0,s.displayRect,new Matrix(),null,java.awt.Color.BLACK,1.0,true,3).getBufferedImage();
  byte[] rgb=new byte[800*600*3];for(int y=0;y<533;y++)for(int x=0;x<800;x++){int v=plate.getRGB(x,y),i=((y+33)*800+x)*3;rgb[i]=(byte)(v>>16);rgb[i+1]=(byte)(v>>8);rgb[i+2]=(byte)v;}
  java.nio.file.Files.write(java.nio.file.Path.of("project/romfs/solar_completed.rgb"),rgb);ImageIO.write(plate,"png",new File("qa/feedback-solar-completed.png"));
  String[] names={"book1","book2","book3","photo1","photo2","photo3","bike_thought","dynamo_fixed","dynamo_free","poles_free","binoculars","meter_zero","meter_half","meter_full","meter_wiring","meter_low"};
  int[] popup={99,110,122,132,142,152,90,90,90,90,90,172,172,172,172,172},first={351,362,373,384,395,406,525,537,549,561,693,99,111,123,135,147};
  for(int i=0;i<names.length;i++)export(names[i],popup[i],"dialogueZoomed",first[i],i==10?35:i<6?11:12);
 }
}
