import java.io.*;import java.awt.*;import java.awt.geom.*;import java.awt.image.BufferedImage;import java.util.*;import java.util.zip.Deflater;
import com.jpexs.decompiler.flash.SWF;import com.jpexs.decompiler.flash.timeline.*;import com.jpexs.decompiler.flash.exporters.commonshape.Matrix;
/* Selection art is a source timeline state, not a coloured circle. The only
 * procedural art here is the wire which AVM1 itself draws with curveTo. */
class ExportPuzzleArt {
 static Timeline root;static SWF s;static BufferedImage keyBare,keyFlat;
 static BufferedImage image()throws Exception{
  BufferedImage raw=SWF.frameToImageGet(root,700,0,null,0,s.displayRect,new Matrix(),null,Color.BLACK,1.0,true,3).getBufferedImage();
  if(keyFlat==null)return raw;
  BufferedImage im=new BufferedImage(800,533,BufferedImage.TYPE_INT_RGB);Graphics2D g=im.createGraphics();g.drawImage(keyFlat,0,0,null);g.dispose();
  for(int y=0;y<533;y++)for(int x=0;x<800;x++)if(raw.getRGB(x,y)!=keyBare.getRGB(x,y))im.setRGB(x,y,raw.getRGB(x,y));
  return im;
 }
 static AffineTransform placement(com.jpexs.decompiler.flash.types.MATRIX m){Matrix p=new Matrix(m);p.translateX/=20;p.translateY/=20;return p.toTransform();}
 static void flattenKeys()throws Exception {
  Timeline ring=ExportPuzzleFeedback.timeline(441);ArrayList<BufferedImage> images=new ArrayList<>();ArrayList<DepthState> instances=new ArrayList<>();
  for(DepthState d:ring.getFrame(0).layers.values())if(s.getCharacter(d.characterId) instanceof com.jpexs.decompiler.flash.tags.DefineSpriteTag){
   Timeline key=ExportPuzzleFeedback.timeline(d.characterId);Matrix pad=new Matrix();pad.translateX=2000;pad.translateY=4000;
   BufferedImage local=SWF.frameToImageGet(key,0,0,null,0,s.displayRect,pad,null,null,2.0,true,3).getBufferedImage();images.add(local);instances.add(d);
   // Flatten each local mask before rotating it. Nested rotated masks in the
   // offline renderer otherwise lose most photographed key bodies.
   key.getFrame(0).layers.entrySet().removeIf(e->e.getValue().characterId!=405&&e.getValue().characterId!=417);
  }
  keyBare=image();BufferedImage flat=new BufferedImage(800,533,BufferedImage.TYPE_INT_RGB);Graphics2D g=flat.createGraphics();g.drawImage(keyBare,0,0,null);g.setRenderingHint(RenderingHints.KEY_INTERPOLATION,RenderingHints.VALUE_INTERPOLATION_BILINEAR);
  DepthState pop=root.getFrame(700).layers.values().iterator().next(),scene=ExportPuzzleFeedback.timeline(753).getFrame(40).layers.get(4),r=null;
  for(DepthState d:ExportPuzzleFeedback.timeline(457).getFrame(0).layers.values())if("ring".equals(d.instanceName))r=d;
  if(r==null)throw new IOException("ring transform");
  // FFDec's render padding is in output pixels, rather than scaled source
  // coordinates. Remove that padding before undoing the local raster scale.
  for(int i=0;i<images.size();i++){AffineTransform m=AffineTransform.getScaleInstance(1.0,1.0);m.concatenate(placement(pop.matrix));m.concatenate(placement(scene.matrix));m.concatenate(placement(r.matrix));m.concatenate(placement(instances.get(i).matrix));m.scale(1/2.0,1/2.0);m.translate(-100,-200);g.drawImage(images.get(i),m,null);}
  g.dispose();keyFlat=flat;
 }
 static void setup(String swf,int popup)throws Exception {
  ExportPuzzleFeedback.s=s=new SWF(new FileInputStream(swf),false);root=ExportPuzzleFeedback.timeline(988);
  var rf=root.getFrame(700);rf.layers.entrySet().removeIf(e->!"popups".equals(e.getValue().instanceName));rf.layers.values().iterator().next().time=popup-1;
  for(DepthState d:ExportPuzzleFeedback.timeline(753).getFrame(popup-1).layers.values()){d.time=0;if(d.characterId==368)d.isVisible=false;}
 }
 static class Lan implements AutoCloseable {
  RandomAccessFile f;int[] offsets;int index;java.nio.file.Path output,temp;
  Lan(String name,int count)throws Exception{output=java.nio.file.Path.of("project/romfs/"+name+".lan");temp=java.nio.file.Path.of(output+".tmp");f=new RandomAccessFile(temp.toFile(),"rw");f.setLength(0);f.writeBytes("LAN2");ExportPuzzleFeedback.le(f,800);ExportPuzzleFeedback.le(f,600);ExportPuzzleFeedback.le(f,25);ExportPuzzleFeedback.le(f,count);offsets=new int[count+1];for(int i=0;i<=count;i++)ExportPuzzleFeedback.le(f,0);}
  void add(BufferedImage im,BufferedImage base)throws Exception {
   int x0=800,y0=600,x1=0,y1=0;
   for(int y=0;y<im.getHeight();y++)for(int x=0;x<800;x++)if(base==null?(im.getRGB(x,y)>>>24)!=0:im.getRGB(x,y)!=base.getRGB(x,y)){x0=Math.min(x0,x);x1=Math.max(x1,x+1);y0=Math.min(y0,y);y1=Math.max(y1,y+1);}
   if(x1<=x0||y1<=y0){offsets[index++]=(int)f.getFilePointer();ExportPuzzleFeedback.le(f,4<<16);for(int k=0;k<4;k++)ExportPuzzleFeedback.sh(f,0);return;}
   offsets[index++]=(int)f.getFilePointer();ExportPuzzleFeedback.le(f,4<<16);ExportPuzzleFeedback.sh(f,x0);ExportPuzzleFeedback.sh(f,y0+(base==null?0:33));ExportPuzzleFeedback.sh(f,x1-x0);ExportPuzzleFeedback.sh(f,y1-y0);
   byte[] raw=new byte[(x1-x0)*(y1-y0)*4];for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++){int c=im.getRGB(x,y),i=((y-y0)*(x1-x0)+x-x0)*4;boolean changed=base==null||c!=base.getRGB(x,y);if(changed){raw[i]=(byte)(c>>16);raw[i+1]=(byte)(c>>8);raw[i+2]=(byte)c;raw[i+3]=base==null?(byte)(c>>>24):(byte)255;}}
   Deflater z=new Deflater(6);z.setInput(raw);z.finish();byte[] b=new byte[raw.length+2048];int n=z.deflate(b);if(!z.finished())throw new IOException("deflate");z.end();f.write(b,0,n);
  }
  public void close()throws Exception{if(index!=offsets.length-1)throw new IOException("frame count");offsets[index]=(int)f.getFilePointer();f.seek(20);for(int o:offsets)ExportPuzzleFeedback.le(f,o);f.getFD().sync();f.close();java.nio.file.Files.move(temp,output,java.nio.file.StandardCopyOption.REPLACE_EXISTING,java.nio.file.StandardCopyOption.ATOMIC_MOVE);}
 }
 static void base(String name,BufferedImage im)throws Exception {
  byte[] rgb=new byte[800*600*3];for(int y=0;y<533;y++)for(int x=0;x<800;x++){int c=im.getRGB(x,y),i=((y+33)*800+x)*3;rgb[i]=(byte)(c>>16);rgb[i+1]=(byte)(c>>8);rgb[i+2]=(byte)c;}
  java.nio.file.Files.write(java.nio.file.Path.of("project/romfs/"+name+".rgb"),rgb);javax.imageio.ImageIO.write(im,"png",new File("qa/art-"+name+"-base.png"));
 }
 public static void main(String[] args)throws Exception {
  setup(args[0],41);Timeline ring=ExportPuzzleFeedback.timeline(441);ArrayList<DepthState> keys=new ArrayList<>();
  for(DepthState d:ring.getFrame(0).layers.values())if(s.getCharacter(d.characterId) instanceof com.jpexs.decompiler.flash.tags.DefineSpriteTag){Timeline key=ExportPuzzleFeedback.timeline(d.characterId);for(DepthState child:key.getFrame(0).layers.values())if(child.characterId==405||child.characterId==417){child.time=0;keys.add(child);}}
  if(keys.size()!=14)throw new IOException("key order");flattenKeys();BufferedImage b=image();base("keys",b);
  try(Lan out=new Lan("art_keys",14)){for(int i=0;i<14;i++){keys.get(i).time=10;BufferedImage im=image();out.add(im,b);javax.imageio.ImageIO.write(im,"png",new File("qa/art-key-"+i+".png"));keys.get(i).time=0;}}
  keyFlat=keyBare=null;setup(args[0],50);Timeline cons=ExportPuzzleFeedback.timeline(515);DepthState[] connectors=new DepthState[12];
  for(DepthState d:cons.getFrame(0).layers.values())if(d.instanceName!=null&&d.instanceName.startsWith("con")){int i=Integer.parseInt(d.instanceName.substring(3))-1;connectors[i]=d;d.time=0;}
  b=image();base("wires",b);
  try(Lan out=new Lan("art_wires",90)) {
   for(int i=0;i<12;i++)for(int state=1;state<=2;state++){connectors[i].time=state;BufferedImage im=image();out.add(im,b);javax.imageio.ImageIO.write(im,"png",new File("qa/art-con-"+i+"-"+state+".png"));connectors[i].time=0;}
   for(int i=0;i<12;i++)for(int j=i+1;j<12;j++) {
    BufferedImage im=new BufferedImage(800,600,BufferedImage.TYPE_INT_ARGB);Graphics2D g=im.createGraphics();g.setRenderingHint(RenderingHints.KEY_ANTIALIASING,RenderingHints.VALUE_ANTIALIAS_ON);g.translate(0,33);g.scale(1.0,1.0);g.setColor(new Color(0x990000));g.setStroke(new BasicStroke(12,BasicStroke.CAP_ROUND,BasicStroke.JOIN_ROUND));
    Path2D.Double path=new Path2D.Double();path.moveTo(connectors[i].matrix.translateX/20.,connectors[i].matrix.translateY/20.);path.quadTo(200,400,connectors[j].matrix.translateX/20.,connectors[j].matrix.translateY/20.);g.draw(path);g.dispose();out.add(im,null);
    if(i==0||j==11)javax.imageio.ImageIO.write(im,"png",new File("qa/art-wire-"+i+"-"+j+".png"));
   }
  }
  System.out.println("PASS: 14 source key states, 24 connector states, 66 AVM1 quadratic strokes; source width 12, colour 0x990000, control (200,400)");
 }
}
