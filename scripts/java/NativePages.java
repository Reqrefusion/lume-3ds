import java.io.*;import java.awt.*;import java.awt.geom.*;import java.awt.image.BufferedImage;import java.nio.file.*;import java.util.*;import javax.imageio.ImageIO;
import com.jpexs.decompiler.flash.timeline.Frame;import com.jpexs.decompiler.flash.SWF;import com.jpexs.decompiler.flash.tags.DefineSpriteTag;import com.jpexs.decompiler.flash.timeline.*;import com.jpexs.decompiler.flash.exporters.commonshape.Matrix;
/* Export authored vector/text/photo states at original stage resolution.
   AVM1 stop, visibility and item state are applied explicitly, not guessed. */
class NativePages {
 static String path;static SWF s;static Timeline root,pop;static Frame pf;
 static Timeline timeline(int id){return ((DefineSpriteTag)s.getCharacter(id)).getTimeline();}
 static void setup(int frame)throws Exception{
  s=new SWF(new FileInputStream(path),false);root=timeline(988);pop=timeline(753);
  Frame rf=root.getFrame(700);rf.layers.entrySet().removeIf(e->!"popups".equals(e.getValue().instanceName));rf.layers.values().iterator().next().time=frame-1;pf=pop.getFrame(frame-1);
  for(DepthState d:pf.layers.values())if(d.characterId==368)d.isVisible=false;
  for(DepthState d:pf.layers.values())if("dialogueZoomed".equals(d.instanceName)||"resetTimerAnim".equals(d.instanceName))d.time=0;
 }
 static BufferedImage image()throws Exception{return SWF.frameToImageGet(root,700,0,null,0,s.displayRect,new Matrix(),null,Color.BLACK,1.0,true,3).getBufferedImage();}
 static void page(String name)throws Exception{ExportPuzzleArt.base(name,image());System.out.println("page "+name);}
 static DepthState named(Timeline t,String name)throws Exception{for(DepthState d:t.getFrame(0).layers.values())if(name.equals(d.instanceName))return d;throw new IOException("Missing "+name);}
 static void locks()throws Exception{
  for(int mode=0;mode<2;mode++){
   setup(mode==0?60:162);Timeline t=timeline(mode==0?553:709);DepthState[] rows={named(t,"row1"),named(t,"row2"),named(t,"row3")};for(DepthState d:rows)d.time=0;
   BufferedImage base=image();String name=mode==0?"cupboard":"door";ExportPuzzleArt.base(name,base);
   try(ExportPuzzleArt.Lan out=new ExportPuzzleArt.Lan("digits_"+name,30)){
    for(int row=0;row<3;row++)for(int digit=0;digit<10;digit++){rows[row].time=digit;out.add(image(),base);rows[row].time=0;}
   }
  }
 }
 static void storage()throws Exception{
  for(int mode=0;mode<2;mode++){
   setup(mode==0?70:90);
   for(int stage=1;stage<=(mode==0?1:3);stage++)for(int mask=0;mask<(mode==0?4:8);mask++){
    for(DepthState d:pf.layers.values()){
     if("objectSpanner".equals(d.instanceName))d.isVisible=(mask&1)==0;
     if("objectWire".equals(d.instanceName))d.isVisible=(mask&2)==0;
     if("binoculars".equals(d.instanceName))d.isVisible=(mask&4)==0;
     if("bike".equals(d.instanceName)){
      d.time=(stage-1)*10;for(DepthState b:timeline(d.characterId).getFrame(d.time).layers.values()){
       if("dynamo".equals(b.instanceName))b.isVisible=(mask&1)==0;if("poles".equals(b.instanceName))b.isVisible=(mask&2)==0;
      }
     }
    }
    page(mode==0?"clue_cupboard_m"+mask:"world_pantry_b"+stage+"_m"+mask);
   }
  }
 }
 static void clues()throws Exception{
  String[] names={"letter","book","book2","photo1","photo2","photo3"};int[] frames={21,99,110,132,142,152};
  for(int i=0;i<names.length;i++){setup(frames[i]);page("clue_"+names[i]);}
  setup(122);DepthState books=null;for(DepthState d:pf.layers.values())if(d.characterId==674)books=d;if(books==null)throw new IOException("books");
  for(int k=0;k<11;k++){books.time=k+1;page("clue_book3_"+k);}
  setup(172);DepthState meter=null;for(DepthState d:pf.layers.values())if("powerMeter".equals(d.instanceName))meter=d;if(meter==null)throw new IOException("meter");
  int[] states={0,130,246};for(int i=0;i<3;i++){meter.time=states[i];page("clue_meter"+i);}
 }
 /* Preserve the authored meter needle, not only its final still image. */
 static void meterTransitions()throws Exception{
  String[] names={"zero_half","half_full"},starts={"zeroToHalf","halfToFull"},ends={"half","full"};
  for(int k=0;k<2;k++){
   setup(172);DepthState meter=null;
   for(DepthState d:pf.layers.values())if("powerMeter".equals(d.instanceName))meter=d;
   if(meter==null)throw new IOException("Missing powerMeter");
   Timeline t=timeline(meter.characterId);int first=-1,last=-1;
   for(Frame f:t.getFrames())for(String label:f.labels){if(label.equals(starts[k]))first=f.frame;if(label.equals(ends[k]))last=f.frame;}
   if(first<0||last<=first)throw new IOException("Missing meter transition labels");
   try(ExportPuzzleArt.Lan out=new ExportPuzzleArt.Lan("ui_meter_"+names[k],last-first+1)){
    for(int f=first;f<=last;f++){
     meter.time=f;BufferedImage canvas=new BufferedImage(800,600,BufferedImage.TYPE_INT_ARGB);
     Graphics2D g=canvas.createGraphics();g.drawImage(image(),0,33,null);g.dispose();out.add(canvas,null);
    }
   }
  }
 }
 static void solar()throws Exception{
  setup(31);page("solar");BufferedImage bitmap=ImageIO.read(s.getImage(11).getConvertedImageData());
  if(bitmap.getWidth()<401||bitmap.getHeight()<402)throw new IOException("solar bitmap");
  try(ExportPuzzleArt.Lan out=new ExportPuzzleArt.Lan("solar_tiles",256)){
   for(int cell=0;cell<64;cell++)for(int rot=0;rot<4;rot++){
    int row=cell/8,col=cell%8;BufferedImage tile=bitmap.getSubimage(1+col*50,2+row*50,50,50);
    BufferedImage im=new BufferedImage(800,600,BufferedImage.TYPE_INT_ARGB);Graphics2D g=im.createGraphics();g.setRenderingHint(RenderingHints.KEY_INTERPOLATION,RenderingHints.VALUE_INTERPOLATION_BICUBIC);
    // Native coordinates mirror the overview grid's exact integer cell bounds.
    double x=(49+col*149/8)*2.5,y=(44+row*149/8)*2.5,w=((col+1)*149/8-col*149/8)*2.5,h=((row+1)*149/8-row*149/8)*2.5;
    g.translate(x+w/2,y+h/2);g.scale(w/50,h/50);g.rotate(rot*Math.PI/2);g.drawImage(tile,-25,-25,null);g.dispose();out.add(im,null);
   }
  }
 }
 static void map()throws Exception{
  setup(80);page("map");Timeline grid=timeline(597);Frame f=grid.getFrame(0);f.layers.entrySet().removeIf(e->!"gridSquare".equals(e.getValue().instanceName));DepthState ink=f.layers.values().iterator().next();ink.time=10;ink.matrix.scaleX=.75f;ink.matrix.scaleY=.75f;ink.matrix.translateX=3837;ink.matrix.translateY=2852;
  Matrix transform=new Matrix();transform.scaleX=.989837646;transform.scaleY=.989837646;transform.rotateSkew0=-.095321655;transform.rotateSkew1=.095306396;transform.translateX=185;transform.translateY=2271;
  // gridSquare.highlighted contains circle (sprite 20); its AVM1 first
  // frame chooses random(4)+1 and stops. Export each authored ink drawing.
  DepthState circle=null;
  for(DepthState d:timeline(21).getFrame(10).layers.values())if(d.characterId==20)circle=d;
  if(circle==null)throw new IOException("map circle child");
  try(ExportPuzzleArt.Lan out=new ExportPuzzleArt.Lan("map_ink",4)){
   for(int variant=0;variant<4;variant++){
    circle.time=variant;
    BufferedImage raw=SWF.frameToImageGet(grid,0,0,null,0,s.displayRect,transform,null,null,1.0,true,3).getBufferedImage();int x0=800,y0=533,x1=0,y1=0;
    for(int y=0;y<raw.getHeight();y++)for(int x=0;x<800;x++)if((raw.getRGB(x,y)>>>24)!=0){x0=Math.min(x0,x);y0=Math.min(y0,y);x1=Math.max(x1,x+1);y1=Math.max(y1,y+1);}
    if(x1<=x0||y1<=y0)throw new IOException("map ink empty");
    BufferedImage im=new BufferedImage(800,600,BufferedImage.TYPE_INT_ARGB);Graphics2D g=im.createGraphics();
    g.drawImage(raw.getSubimage(x0,y0,x1-x0,y1-y0),400-(x1-x0)/2,300-(y1-y0)/2,null);g.dispose();out.add(im,null);
   }
  }
 }
 public static void main(String[]a)throws Exception{
  path=a[0];if(a.length>1&&a[1].equals("--smoke")){setup(99);page("clue_book");return;}
  if(a.length>1&&a[1].equals("--meters")){meterTransitions();return;}
  if(a.length>1&&a[1].equals("--locks")){locks();return;}
  clues();meterTransitions();storage();locks();solar();map();setup(194);page("piano");
  setup(31);for(DepthState d:pf.layers.values())if("solarPanel".equals(d.instanceName))d.isVisible=false;page("solar_completed");
 }
}
