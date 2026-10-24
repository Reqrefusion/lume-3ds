import com.jpexs.decompiler.flash.SWF;
import com.jpexs.decompiler.flash.tags.DefineSpriteTag;
import com.jpexs.decompiler.flash.tags.base.TextTag;
import com.jpexs.decompiler.flash.tags.base.StaticTextTag;
import com.jpexs.decompiler.flash.tags.base.FontTag;
import com.jpexs.decompiler.flash.types.TEXTRECORD;
import com.jpexs.decompiler.flash.timeline.*;
import com.jpexs.decompiler.flash.exporters.shape.PathExporter;
import com.jpexs.decompiler.flash.tags.base.ShapeTag;
import java.awt.geom.*;import java.awt.image.BufferedImage;import java.awt.Graphics2D;import java.awt.RenderingHints;import java.awt.Color;
import java.io.*;import java.nio.file.*;import java.nio.charset.StandardCharsets;import java.text.Normalizer;import java.util.*;
/* Read embedded characters directly. TextTag already resolves SWF glyph
   indices; page wrapping is performed by the actual 3DS screen width in C. */
public class ReadingText {
 static String clean(String s){s=s.replace('\u2018','\'').replace('\u2019','\'').replace('\u201c','"').replace('\u201d','"').replace('\u2013','-').replace('\u2014','-').replace("\u2026","...");s=Normalizer.normalize(s,Normalizer.Form.NFKD).replaceAll("\\p{M}","").replaceAll("\\s+"," ").trim();for(char c:s.toCharArray())if(c<32||c>126)throw new IllegalArgumentException("Unsupported original character: "+(int)c);return s;}
 static String original(TextTag tag,SWF swf)throws IOException {
  if(!(tag instanceof StaticTextTag))return String.join("",tag.getTexts());
  StringBuilder text=new StringBuilder();FontTag font=null;int x=0,y=0,height=400;
  for(TEXTRECORD rec:((StaticTextTag)tag).textRecords){
   if(rec.styleFlagsHasFont){font=rec.getFont(swf);height=rec.textHeight;}
   if(font==null)throw new IOException("Missing embedded speech font");
   int nx=rec.styleFlagsHasXOffset?rec.xOffset:x,ny=rec.styleFlagsHasYOffset?rec.yOffset:y;
   if(text.length()>0&&(ny!=y||nx>x+height/10))text.append(' ');
   text.append(rec.getText(font));x=nx;y=ny;
   for(var glyph:rec.glyphEntries)x+=glyph.glyphAdvance;
  }return text.toString();
 }
 static void fontAtlas(SWF swf,FontTag font,Path output)throws Exception {
  byte[] pixels=new byte[95*12*20];double units=1024.0*font.getDivider(),ratio=20.0/units;
  java.awt.Shape[] shapes=new java.awt.Shape[95];double maxW=0,minY=0,maxY=0;
  for(int c=32;c<=126;c++){int index=font.charToGlyph((char)c);if(index<0)continue;
   Area shape=new Area();for(GeneralPath path:PathExporter.export(ShapeTag.WIND_EVEN_ODD,1,swf,font.getGlyphShapeTable().get(index),new ArrayList<>()))shape.add(new Area(path));
   shapes[c-32]=shape;Rectangle2D b=shape.getBounds2D();maxW=Math.max(maxW,b.getWidth());minY=Math.min(minY,b.getMinY());maxY=Math.max(maxY,b.getMaxY());
  }
  if(maxW==0||maxY<=minY)throw new IOException("Embedded speech font has no outlines");
  ratio=Math.min(ratio,Math.min(10/maxW,18/(maxY-minY)));
  for(int c=32;c<=126;c++){java.awt.Shape shape=shapes[c-32];if(shape==null)continue;BufferedImage im=new BufferedImage(12,20,BufferedImage.TYPE_INT_ARGB);Graphics2D g=im.createGraphics();g.setRenderingHint(RenderingHints.KEY_ANTIALIASING,RenderingHints.VALUE_ANTIALIAS_ON);g.setColor(Color.BLACK);Rectangle2D b=shape.getBounds2D();g.translate(1-b.getX()*ratio,1-minY*ratio);g.scale(ratio,ratio);g.fill(shape);g.dispose();for(int y=0;y<20;y++)for(int x=0;x<12;x++)pixels[(c-32)*240+y*12+x]=(byte)(im.getRGB(x,y)>>>24);}
  try(OutputStream f=Files.newOutputStream(output)){f.write(new byte[]{'L','R','F','1',12,20,95,0});f.write(pixels);}
 }
 /* Keep the actual source paper shapes/bitmap fills and their transforms.
    Speech glyphs/debug controls occupy the upper layers; remove these before
    rendering the blank template. No OCR or fabricated paper texture is used. */
 static void paper(SWF swf,Timeline timeline,Path out)throws Exception {
  int first=0,last=0;
  for(Frame frame:timeline.getFrames())if(frame.labels.contains("template")){
   first=frame.frame;last=timeline.getFrameCount()-1;
   for(int i=first+1;i<timeline.getFrameCount();i++)if(!timeline.getFrame(i).labels.isEmpty()){last=i-1;break;}
   break;
  }
  for(Frame frame:timeline.getFrames()){
   frame.layers.entrySet().removeIf(e->e.getKey()>=10);
   for(DepthState layer:frame.layers.values())if(layer.characterId==232)layer.time=Math.min(layer.time,1);
  }
  BufferedImage source=null;
  for(int frame=last;frame>=first;frame--){
   BufferedImage image=SWF.frameToImageGet(timeline,frame,0,null,0,swf.displayRect,
    new com.jpexs.decompiler.flash.exporters.commonshape.Matrix(),null,null,1.0,true,3).getBufferedImage();
   int x0=image.getWidth(),y0=image.getHeight(),x1=0,y1=0;
   for(int y=0;y<image.getHeight();y++)for(int x=0;x<image.getWidth();x++)if((image.getRGB(x,y)>>>24)>1){x0=Math.min(x0,x);y0=Math.min(y0,y);x1=Math.max(x1,x+1);y1=Math.max(y1,y+1);}
   if(x1-x0>=32&&y1-y0>=24){source=image.getSubimage(x0,y0,x1-x0,y1-y0);break;}
  }
  if(source==null)throw new IOException("Original blank speech paper not found in template frames "+first+".."+last);
  for(int width:new int[]{320,400}){
   BufferedImage image=new BufferedImage(width,240,BufferedImage.TYPE_INT_ARGB);Graphics2D g=image.createGraphics();
   g.setRenderingHint(RenderingHints.KEY_INTERPOLATION,RenderingHints.VALUE_INTERPOLATION_BILINEAR);
   g.drawImage(source,8,8,width-16,200,null);g.dispose();
   try(OutputStream f=new BufferedOutputStream(Files.newOutputStream(out.resolve("paper"+width+".bin")))){
    f.write(new byte[]{'L','R','P','2',(byte)width,(byte)(width>>>8),(byte)240,0});
    // Premultiplied RGB followed by alpha: no baked background color.
    for(int y=0;y<240;y++)for(int x=0;x<width;x++){int rgb=image.getRGB(x,y),alpha=rgb>>>24;f.write(((rgb>>>16&255)*alpha+127)/255);f.write(((rgb>>>8&255)*alpha+127)/255);f.write(((rgb&255)*alpha+127)/255);}
    for(int y=0;y<240;y++)for(int x=0;x<width;x++)f.write(image.getRGB(x,y)>>>24);
   }
  }
  System.out.println("Original blank speech paper: 320x240 + 400x240");
 }
 /* Search only direct dialogue layers: child sprites include authoring/debug
    controls such as "speech timer". Recursing into them would invent dialogue.
    The closing frame may have removed the text, so use the latest nonempty
    frame inside this label, never a frame belonging to the next label. */
 static List<TextTag> dialogue(SWF swf,Timeline timeline,int first,int last)throws IOException {
  for(int frame=Math.min(last,timeline.getFrameCount()-1);frame>=first;frame--){
   List<TextTag> tags=new ArrayList<>();
   for(DepthState d:new TreeMap<>(timeline.getFrame(frame).layers).values())
    if(d.depth>=10&&swf.getCharacter(d.characterId) instanceof TextTag){
     TextTag tag=(TextTag)swf.getCharacter(d.characterId);
     if(!clean(original(tag,swf)).isEmpty())tags.add(tag);
    }
   if(!tags.isEmpty())return tags;
  }
  return Collections.emptyList();
 }
 public static void main(String[]a)throws Exception{
  SWF swf=new SWF(new FileInputStream(a[0]),false);Timeline t=((DefineSpriteTag)swf.getCharacter(310)).getTimeline();Path out=Path.of("project/romfs/reading");Files.createDirectories(out);int count=0;Map<Integer,Integer> fonts=new HashMap<>();
  // Only this derived catalogue is cleared; source media are never touched.
  try(DirectoryStream<Path> previous=Files.newDirectoryStream(out,"*.txt")){for(Path file:previous)Files.delete(file);}
  for(Frame first:t.getFrames())for(String name:first.labels){
   if(name.equals("static"))continue;
   if(!name.matches("[A-Za-z][A-Za-z0-9_]{0,63}"))throw new IOException("Unsafe label");
   int last=t.getFrameCount()-1;for(int i=first.frame+1;i<t.getFrameCount();i++)if(!t.getFrame(i).labels.isEmpty()){last=i-1;break;}
   if(name.equals("rotationPuzzleWin"))last=Math.min(last,747);
   StringBuilder text=new StringBuilder();
   for(TextTag tag:dialogue(swf,t,first.frame,last)){
    if(text.length()>0)text.append(' ');text.append(original(tag,swf));
    if(tag instanceof StaticTextTag)for(TEXTRECORD rec:((StaticTextTag)tag).textRecords)if(rec.styleFlagsHasFont)fonts.merge(rec.fontId,rec.glyphEntries.size(),Integer::sum);
   }
   String original=clean(text.toString());
   // Empty catalog entries intentionally keep the existing graphical balloon.
   // Do not classify an unextracted label as an empty original conversation.
   if(original.isEmpty())System.out.println("GORSEL YEDEK: "+name+" (frames "+first.frame+".."+last+", direct dialogue unavailable)");
   if(original.length()>4095)throw new IOException("Dialogue exceeds reader bounds: "+name);
   Files.writeString(out.resolve(name+".txt"),original+"\n",StandardCharsets.US_ASCII);System.out.println(name+" "+original.length());count++;
  }
  if(fonts.isEmpty())throw new IOException("No embedded speech font found");
  int font=Collections.max(fonts.entrySet(),Map.Entry.comparingByValue()).getKey();fontAtlas(swf,swf.getFont(font),out.resolve("font.bin"));
  if(count!=59)throw new IOException("Unexpected dialogue label count: "+count);
  paper(swf,t,out);
 }
}
