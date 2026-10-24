import java.io.*;import java.awt.Color;import java.awt.image.BufferedImage;import java.util.zip.Deflater;
import com.jpexs.decompiler.flash.SWF;import com.jpexs.decompiler.flash.timeline.*;import com.jpexs.decompiler.flash.exporters.commonshape.Matrix;
/* Keyboard presses animate independently, including overlapping presses.
 * Bake only pixels changed from the static source popup, so the photographed
 * paper and key edges are not alpha-blended onto themselves a second time. */
class ExportPianoEffects {
 static BufferedImage image(Timeline root,SWF s)throws Exception{return SWF.frameToImageGet(root,700,0,null,0,s.displayRect,new Matrix(),null,Color.BLACK,1.0,true,3).getBufferedImage();}
 public static void main(String[] args)throws Exception {
  for(int key=0;key<9;key++) {
   ExportPuzzleFeedback.s=new SWF(new FileInputStream(args[0]),false);SWF s=ExportPuzzleFeedback.s;
   Timeline root=ExportPuzzleFeedback.timeline(988);Frame rf=root.getFrame(700);rf.layers.entrySet().removeIf(e->!"popups".equals(e.getValue().instanceName));rf.layers.values().iterator().next().time=193;
   // The popup itself stays on its piano frame. Stop other feedback children.
   for(DepthState d:ExportPuzzleFeedback.timeline(753).getFrame(193).layers.values())if("resetTimerAnim".equals(d.instanceName)||"dialogueZoomed".equals(d.instanceName))d.time=0;
   DepthState target=null;String name=key==8?"btnClear":"keyAnim"+(key+1);
   for(DepthState d:ExportPuzzleFeedback.timeline(738).getFrame(0).layers.values())if(name.equals(d.instanceName))target=d;
   if(target==null)throw new IOException(name);target.time=0;BufferedImage base=image(root,s);
   int first=key==8?7:8,count=key==8?14:27;
   String asset="ui_piano_"+(key==8?"clear":"key"+(key+1));
   try(RandomAccessFile f=new RandomAccessFile("project/romfs/"+asset+".lan","rw")) {
    f.setLength(0);f.writeBytes("LAN2");ExportPuzzleFeedback.le(f,800);ExportPuzzleFeedback.le(f,600);ExportPuzzleFeedback.le(f,25);ExportPuzzleFeedback.le(f,count);for(int k=0;k<=count;k++)ExportPuzzleFeedback.le(f,0);int[] offsets=new int[count+1];
    boolean changed=false;
    for(int k=0;k<count;k++) {
     target.time=k==count-1?0:first+k;BufferedImage im=image(root,s);
     int x0=800,y0=533,x1=0,y1=0;
     for(int y=0;y<533;y++)for(int x=0;x<800;x++)if(im.getRGB(x,y)!=base.getRGB(x,y)){x0=Math.min(x0,x);x1=Math.max(x1,x+1);y0=Math.min(y0,y);y1=Math.max(y1,y+1);}
     offsets[k]=(int)f.getFilePointer();ExportPuzzleFeedback.le(f,4<<16);
     if(x1<=x0||y1<=y0){ExportPuzzleFeedback.sh(f,0);ExportPuzzleFeedback.sh(f,0);ExportPuzzleFeedback.sh(f,0);ExportPuzzleFeedback.sh(f,0);}
     else {changed=true;int w=x1-x0,h=y1-y0;ExportPuzzleFeedback.sh(f,x0);ExportPuzzleFeedback.sh(f,y0+33);ExportPuzzleFeedback.sh(f,w);ExportPuzzleFeedback.sh(f,h);byte[] raw=new byte[w*h*4];
      for(int y=0;y<h;y++)for(int x=0;x<w;x++){int v=im.getRGB(x+x0,y+y0),i=(y*w+x)*4;if(v!=base.getRGB(x+x0,y+y0)){raw[i]=(byte)(v>>16);raw[i+1]=(byte)(v>>8);raw[i+2]=(byte)v;raw[i+3]=(byte)255;}}
      Deflater z=new Deflater(6);z.setInput(raw);z.finish();byte[] b=new byte[raw.length+2048];int n=z.deflate(b);if(!z.finished())throw new IOException("deflate");z.end();f.write(b,0,n);
     }
     if(k==3){javax.imageio.ImageIO.write(base,"png",new File("qa/"+asset+"-base.png"));javax.imageio.ImageIO.write(im,"png",new File("qa/"+asset+"-source.png"));}
    }
    if(!changed)throw new IOException("Empty effect: "+name);
    offsets[count]=(int)f.getFilePointer();f.seek(20);for(int o:offsets)ExportPuzzleFeedback.le(f,o);
   }
   System.out.println(asset+" "+count);
  }
 }
}
