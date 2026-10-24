import com.jpexs.decompiler.flash.SWF;
import com.jpexs.decompiler.flash.tags.DefineSpriteTag;
import com.jpexs.decompiler.flash.timeline.*;
import com.jpexs.decompiler.flash.timeline.Frame;
import com.jpexs.decompiler.flash.exporters.commonshape.Matrix;
import com.jpexs.decompiler.flash.types.RECT;
import java.io.*;import java.nio.file.*;import java.awt.*;import java.awt.image.BufferedImage;import java.util.*;import java.util.zip.Deflater;
/* Offline source display-list rendering. Root and child frame selections come
   from the verified original timeline, including authored gotoAndStop tails. */
public class SourceGame {
 static SWF swf;static String input;static Path out,requests;
 static Timeline t(int id){return ((DefineSpriteTag)swf.getCharacter(id)).getTimeline();}
 static void load()throws Exception{swf=new SWF(new FileInputStream(input),false);
  for(var tag:swf.getCharacters(false).values())if(tag instanceof DefineSpriteTag sp)for(Frame f:sp.getTimeline().getFrames())f.layers.entrySet().removeIf(e->{String n=e.getValue().instanceName;return n!=null&&(n.startsWith("hotspot")||n.equals("btnSkip"));});
  Map<Integer,Integer> stops=new HashMap<>();for(String line:Files.readAllLines(requests.resolve("stops.tsv"))){String[]v=line.split("\t");stops.put(Integer.parseInt(v[0]),Integer.parseInt(v[1])-1);}for(var tag:swf.getCharacters(false).values())if(tag instanceof DefineSpriteTag sp)for(Frame f:sp.getTimeline().getFrames())for(DepthState d:f.layers.values())if(stops.containsKey(d.characterId))d.time=Math.min(d.time,stops.get(d.characterId));
  for(Frame f:t(310).getFrames())for(DepthState d:f.layers.values())if(d.characterId==232)d.time=Math.min(d.time,1);
 }
 static BufferedImage stage(Timeline tl,int fr,boolean high)throws Exception{
  double z=high?1:.4;BufferedImage src=SWF.frameToImageGet(tl,fr,0,null,0,swf.displayRect,new Matrix(),null,null,z,true,3).getBufferedImage();
  BufferedImage dst=new BufferedImage(high?800:320,high?600:240,BufferedImage.TYPE_INT_ARGB);Graphics2D g=dst.createGraphics();g.drawImage(src,0,high?33:13,null);g.dispose();return dst;
 }
 static byte[] pixels(BufferedImage im,boolean rgb){int n=rgb?3:4;byte[]b=new byte[im.getWidth()*im.getHeight()*n];for(int y=0;y<im.getHeight();y++)for(int x=0;x<im.getWidth();x++){int p=im.getRGB(x,y),i=(y*im.getWidth()+x)*n;b[i]=(byte)(p>>16);b[i+1]=(byte)(p>>8);b[i+2]=(byte)p;if(!rgb)b[i+3]=(byte)(p>>>24);}return b;}
 static Rectangle box(BufferedImage im){int a=im.getWidth(),b=im.getHeight(),c=0,d=0;for(int y=0;y<im.getHeight();y++)for(int x=0;x<im.getWidth();x++)if((im.getRGB(x,y)>>>24)!=0){a=Math.min(a,x);b=Math.min(b,y);c=Math.max(c,x+1);d=Math.max(d,y+1);}return new Rectangle(a,b,Math.max(0,c-a),Math.max(0,d-b));}
 static void le(RandomAccessFile f,int x)throws Exception{f.writeInt(Integer.reverseBytes(x));}static void sh(RandomAccessFile f,int x)throws Exception{f.writeShort(Short.reverseBytes((short)x));}
 static class Lan implements AutoCloseable{
  RandomAccessFile f;int[]ix;int k;Path path,tmp;
  Lan(Path p,int w,int h,int n)throws Exception{path=p;Files.createDirectories(p.getParent());tmp=Path.of(p+".tmp");f=new RandomAccessFile(tmp.toFile(),"rw");f.setLength(0);f.writeBytes("LAN2");le(f,w);le(f,h);le(f,25);le(f,n);ix=new int[n+1];for(int i=0;i<=n;i++)le(f,0);}
  void add(BufferedImage im,int bg)throws Exception{Rectangle r=box(im);ix[k++]=(int)f.getFilePointer();le(f,bg);sh(f,r.width==0?0:r.x);sh(f,r.height==0?0:r.y);sh(f,r.width);sh(f,r.height);if(r.isEmpty())return;byte[]raw=pixels(im.getSubimage(r.x,r.y,r.width,r.height),false);Deflater z=new Deflater(6);z.setInput(raw);z.finish();byte[]b=new byte[raw.length+4096];int n=z.deflate(b);if(!z.finished())throw new IOException("deflate capacity");z.end();f.write(b,0,n);}
  public void close()throws Exception{if(k!=ix.length-1){f.close();throw new IOException("frame count "+path);}ix[k]=(int)f.getFilePointer();f.seek(20);for(int x:ix)le(f,x);f.getFD().sync();f.close();Files.move(tmp,path,StandardCopyOption.REPLACE_EXISTING);}
 }
 static boolean photoPlate(DepthState d){return d.characterId==910||d.characterId==915||d.characterId==917;}
 static Set<Integer> depths(String field){Set<Integer> result=new HashSet<>();if(!field.equals("-"))for(String x:field.split(","))result.add(Integer.parseInt(x));return result;}
 /* The native reader owns both the text and paper of cinematic speech.
    Remove the verified paper body and its shadow at their authored depths;
    retain shape 963, the small tail pointing at the speaker.
    character silhouettes, face masks and photographic layers stay intact. */
 static void readableEnding(Timeline root) {
  int[] papers={0},tails={0};
  for(int frame=2537;frame<=4444;frame++)root.getFrame(frame).layers.entrySet().removeIf(e->{
   DepthState d=e.getValue();int id=d.characterId;
   if(d.depth==330&&id==963)tails[0]++;
   boolean paper=(d.depth==329&&id==224)||(d.depth==331&&id==235);
   if(paper)papers[0]++;
   return paper||(id>=964&&id<=972&&swf.getCharacter(id) instanceof com.jpexs.decompiler.flash.tags.base.TextTag)
     ||(id>=974&&id<=982);
  });
  if(papers[0]!=1174||tails[0]!=587)throw new IllegalStateException("Ending paper/tail layout changed: "+papers[0]+"/"+tails[0]);
 }
 /* New-game intro keeps stage1, exactly as setUpConstants/setTurbine.
    Construction is controlled by game state, never by the film clock. */
 static void introInitialProps(Timeline root) {
  for(int frame=1751;frame<=2535;frame++)for(DepthState d:root.getFrame(frame).layers.values())
   if(d.characterId==208){d.time=0;d.isVisible=false;}
 }
 /* The source AS stops the construction timeline at stage5 (frame42).
    Its blade sprite keeps playing independently. Display-list export must
    reproduce both clocks instead of looping all 54 construction frames. */
 static void endingFinalProps(Timeline root) {
  for(int frame=2537;frame<=4444;frame++)for(DepthState d:root.getFrame(frame).layers.values())
   if(d.characterId==208)d.time=41;
   else if(d.characterId==216)d.isVisible=false;
  for(Frame f:t(213).getFrames())f.layers.entrySet().removeIf(e->"blades".equals(e.getValue().instanceName));
 }
 static void endingRotorFrame(int frame) {
  for(DepthState d:t(208).getFrame(41).layers.values())if(d.characterId==206)
   d.time=(frame-2537)%t(206).getFrameCount();
 }
 static boolean actor(DepthState d){return d.depth>=157&&d.depth<=270&&d.characterId!=762&&d.characterId!=349;}
 static void clip(Path request)throws Exception{
  load();Timeline root=t(988);var lines=Files.readAllLines(request);String mode=lines.get(0),name=request.getFileName().toString().replace(".tsv","");int count=lines.size()-1,lastRoot=0,lastChild=-1;BufferedImage last=null;
  if(name.equals("anim_ending")){readableEnding(root);endingFinalProps(root);}
  if(name.equals("anim_intro"))introInitialProps(root);
  for(int j=1;j<lines.size();j++){String[]v=lines.get(j).split("\t");Frame f=root.getFrame(Integer.parseInt(v[0])-1);Set<Integer> backdrop=v.length>5?depths(v[5]):null;f.layers.entrySet().removeIf(e->{DepthState d=e.getValue();if(mode.equals("cinema"))return d.characterId==175||d.characterId==740||d.characterId==940||d.characterId==945||(photoPlate(d)&&(backdrop==null||backdrop.contains(d.depth)))||d.characterId==753;return !actor(d);});}
  try(Lan lan=new Lan(out.resolve(name+".lan"),320,240,count)){
   for(int i=1;i<lines.size();i++){String[]v=lines.get(i).split("\t");int frame=Integer.parseInt(v[0])-1,child=Integer.parseInt(v[1]),min=Integer.parseInt(v[2]),bg=(Integer.parseInt(v[3])<<16)|Integer.parseInt(v[4]);Frame f=root.getFrame(frame);
    if(child>=0){f.layers.entrySet().removeIf(e->!"you".equals(e.getValue().instanceName));if(f.layers.isEmpty())throw new IOException("Missing source actor "+name);DepthState d=f.layers.values().iterator().next();d.time=child;for(Frame cf:t(d.characterId).getFrames())cf.layers.entrySet().removeIf(e->{String n=e.getValue().instanceName;return (e.getKey()<min&&e.getValue().characterId!=226)||n!=null&&(n.startsWith("dialogue")||n.equals("gramophone"));});}
    if(name.equals("anim_ending"))endingRotorFrame(frame);
    last=stage(root,frame,false);lan.add(last,bg);lastRoot=frame;lastChild=child;
    if(i%100==0)System.out.println(name+" "+i+"/"+count);
   }
  }
  if(!mode.equals("cinema"))try(Lan hi=new Lan(out.resolve("detail/actor_"+name+".lan"),800,600,1)){hi.add(stage(root,lastRoot,true),4<<16);}
  System.out.println("source clip "+name+" "+count);
 }
 static void plates()throws Exception{load();Timeline root=t(988);for(String l:Files.readAllLines(requests.resolve("plates.tsv"))){String[]v=l.split("\t");int id=Integer.parseInt(v[0]),fr=Integer.parseInt(v[1])-1;Frame f=root.getFrame(fr);var saved=new TreeMap<>(f.layers);Set<Integer> backdrop=v.length>2?depths(v[2]):null;f.layers.entrySet().removeIf(e->!photoPlate(e.getValue())||(backdrop!=null&&!backdrop.contains(e.getKey())));BufferedImage im=stage(root,fr,false);Files.write(out.resolve("cinema_bg"+id+".rgb"),pixels(im,true));Files.createDirectories(out.resolve("detail"));Files.write(out.resolve("detail/cinema_bg"+id+".rgb"),pixels(stage(root,fr,true),true));f.layers.clear();f.layers.putAll(saved);}}
 static void props()throws Exception{
  int[]ids={208,208,208,208,208,213,213,216};int[]frames={0,10,20,30,41,0,0,0};String[]names={"turbine1","turbine2","turbine3","turbine4","turbine5","fence0","fence1","solar"};
  for(int i=0;i<ids.length;i++){load();DefineSpriteTag sp=(DefineSpriteTag)swf.getCharacter(ids[i]);Timeline tl=sp.getTimeline();if(names[i].equals("fence0"))for(Frame f:tl.getFrames())f.layers.entrySet().removeIf(e->"blades".equals(e.getValue().instanceName));RECT r=sp.getRect();double x=r.Xmin/20.,y=r.Ymin/20.,w=(r.Xmax-r.Xmin)/20.,h=(r.Ymax-r.Ymin)/20.,z=Math.min(2,Math.min(304/Math.max(1,w),224/Math.max(1,h)));Matrix m=new Matrix();m.scaleX=m.scaleY=z;m.translateX=(8-x*z)*20;m.translateY=(8-y*z)*20;BufferedImage im=SWF.frameToImageGet(tl,frames[i],0,null,0,new RECT(0,6400,0,4800),m,null,null,1,true,3).getBufferedImage();Rectangle b=box(im);if(b.isEmpty())throw new IOException("Empty prop "+names[i]);
   try(RandomAccessFile f=new RandomAccessFile(out.resolve("prop_"+names[i]+".bin").toFile(),"rw")){f.setLength(0);f.writeBytes("PRP1");le(f,b.width);le(f,b.height);le(f,Float.floatToIntBits((float)(x+(b.x-8)/z)));le(f,Float.floatToIntBits((float)(y+(b.y-8)/z)));le(f,Float.floatToIntBits((float)(1/z)));f.write(pixels(im.getSubimage(b.x,b.y,b.width,b.height),false));}
  }
 }
 static void gramophone()throws Exception{load();Timeline root=t(988);Frame f=root.getFrame(969);f.layers.entrySet().removeIf(e->!"you".equals(e.getValue().instanceName));DepthState you=f.layers.values().iterator().next();you.time=0;Frame a=t(886).getFrame(0);a.layers.entrySet().removeIf(e->!"gramophone".equals(e.getValue().instanceName));DepthState g=a.layers.values().iterator().next();try(Lan lan=new Lan(out.resolve("fx_gramophone.lan"),320,240,214)){for(int i=0;i<214;i++){g.time=10+i;lan.add(stage(root,969,false),4<<16);}}}
 static void extras()throws Exception{
  load();BufferedImage close=NativeMenuArt.source(swf,368,0,false),small=new BufferedImage(18,18,BufferedImage.TYPE_INT_ARGB);Graphics2D g=small.createGraphics();g.setRenderingHint(RenderingHints.KEY_INTERPOLATION,RenderingHints.VALUE_INTERPOLATION_BICUBIC);g.drawImage(close,0,0,18,18,null);g.dispose();Files.write(out.resolve("ui_close.rgba"),pixels(small,false));
  var bitmap=javax.imageio.ImageIO.read(swf.getImage(11).getConvertedImageData());javax.imageio.ImageIO.write(bitmap,"png",out.resolve("solar_source.png").toFile());
  Timeline speech=t(310);ArrayList<Integer>starts=new ArrayList<>();ArrayList<String>labels=new ArrayList<>();for(Frame f:speech.getFrames())for(String l:f.labels){starts.add(f.frame);labels.add(l);}
  for(int k=0;k<labels.size();k++){int fr=k+1<starts.size()?starts.get(k+1)-1:speech.getFrameCount()-1;Matrix m=new Matrix();m.scaleX=.271994;m.rotateSkew0=.008386;m.rotateSkew1=-.01111;m.scaleY=.271912;m.translateX=114.865*20;m.translateY=5.115*20;
   BufferedImage im=SWF.frameToImageGet(speech,fr,0,null,0,new RECT(0,6400,0,4800),m,null,null,1,true,3).getBufferedImage();Files.write(out.resolve("speech_"+labels.get(k)+".rgba"),pixels(im,false));
   m.scaleX*=2.5;m.scaleY*=2.5;m.rotateSkew0*=2.5;m.rotateSkew1*=2.5;m.translateX*=2.5;m.translateY*=2.5;BufferedImage hi=SWF.frameToImageGet(speech,fr,0,null,0,new RECT(0,16000,0,12000),m,null,null,1,true,3).getBufferedImage();try(Lan lan=new Lan(out.resolve("detail/speech_"+labels.get(k)+".lan"),800,600,1)){lan.add(hi,4<<16);}
  }Files.writeString(out.resolve("speech_labels.txt"),String.join("\n",labels)+"\n");
 }
 public static void main(String[]args)throws Exception{input=args[0];requests=Path.of(args[1]);out=Path.of("project/romfs");Files.createDirectories(out.resolve("detail"));String phase=args.length>2?args[2]:"all";
  if(phase.equals("ending"))clip(requests.resolve("anim_ending.tsv"));
  if(phase.equals("intro"))clip(requests.resolve("anim_intro.tsv"));
  if(phase.equals("all")||phase.equals("clips"))try(var paths=Files.list(requests)){for(Path p:paths.filter(p->p.toString().endsWith(".tsv")&&!p.getFileName().toString().equals("plates.tsv")&&!p.getFileName().toString().equals("stops.tsv")).sorted().toList()){clip(p);System.gc();}}
  if(phase.equals("all")||phase.equals("props")){plates();props();gramophone();}
  if(phase.equals("all")||phase.equals("extras"))extras();
 }
}
