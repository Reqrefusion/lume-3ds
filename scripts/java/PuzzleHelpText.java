import com.jpexs.decompiler.flash.SWF;
import com.jpexs.decompiler.flash.tags.base.TextTag;
import com.jpexs.decompiler.flash.tags.DefineSpriteTag;
import com.jpexs.decompiler.flash.timeline.*;
import java.io.*;import java.nio.file.*;import java.nio.charset.StandardCharsets;
/* Read only the displayed instruction labels audited in the source popup.
   Never recurse into authoring counters, Cheat controls or solution data. */
public class PuzzleHelpText {
 static String text(SWF swf,int id)throws Exception {
  if(!(swf.getCharacter(id) instanceof TextTag))throw new IOException("Original instruction text missing: "+id);
  TextTag tag=(TextTag)swf.getCharacter(id);StringBuilder lines=new StringBuilder();
  if(tag instanceof com.jpexs.decompiler.flash.tags.base.StaticTextTag st){
   com.jpexs.decompiler.flash.tags.base.FontTag font=null;int y=0,delta=0;boolean first=true;
   for(var r:st.textRecords){
    if(r.styleFlagsHasFont)font=r.getFont(swf);
    if(font==null)throw new IOException("Instruction font missing: "+id);
    int ny=r.styleFlagsHasYOffset?r.yOffset:y;
    if(!first&&ny!=y){
     lines.append('\n');
     if(delta>0&&ny-y>delta*3/2)lines.append('\n');
     if(ny>y&&(delta==0||ny-y<delta))delta=ny-y;
    }
    lines.append(r.getText(font));y=ny;first=false;
   }
  }else lines.append(String.join("\n",tag.getTexts()));
  StringBuilder clean=new StringBuilder();
  for(String line:lines.toString().split("\n",-1)){
   if(clean.length()>0)clean.append('\n');clean.append(ReadingText.clean(line));
  }
  String s=clean.toString();
  if(s.isEmpty())throw new IOException("Original instruction text empty: "+id);return s;
 }
 static void check(SWF swf,int sprite,int id)throws IOException {
  Timeline t=((DefineSpriteTag)swf.getCharacter(sprite)).getTimeline();
  if(t.getFrame(0).layers.values().stream().noneMatch(d->d.characterId==id))throw new IOException("Instruction is not on its original popup: "+id);
 }
 public static void main(String[]args)throws Exception {
  SWF swf=new SWF(new FileInputStream(args[0]),false);Path out=Path.of("project/romfs/reading");Files.createDirectories(out);
  for(int[]row:new int[][]{{388,379,377},{597,594,570}}){
   check(swf,row[0],row[1]);check(swf,row[0],row[2]);
   String label=row[0]==388?"help_solar":"help_map";
   String text=text(swf,row[1])+"\n\n"+text(swf,row[2]);
   if(text.length()>4095)throw new IOException("Instruction reader bounds: "+label);
   Files.writeString(out.resolve(label+".txt"),text+"\n",StandardCharsets.US_ASCII);
   System.out.println(label+": "+text);
  }
  /* Reuse the same embedded typeface and source paper as ordinary speech.
     Standalone installation also works before the reading-balloon generator. */
  Timeline speech=((DefineSpriteTag)swf.getCharacter(310)).getTimeline();
  java.util.Map<Integer,Integer> fonts=new java.util.HashMap<>();
  for(Frame first:speech.getFrames())for(String label:first.labels){
   if(label.equals("static"))continue;
   int last=speech.getFrameCount()-1;
   for(int i=first.frame+1;i<speech.getFrameCount();i++)if(!speech.getFrame(i).labels.isEmpty()){last=i-1;break;}
   if(label.equals("rotationPuzzleWin"))last=Math.min(last,747);
   for(TextTag tag:ReadingText.dialogue(swf,speech,first.frame,last))
    if(tag instanceof com.jpexs.decompiler.flash.tags.base.StaticTextTag st)
     for(var r:st.textRecords)if(r.styleFlagsHasFont)fonts.merge(r.fontId,r.glyphEntries.size(),Integer::sum);
  }
  if(fonts.isEmpty())throw new IOException("Original speech font missing");
  int font=java.util.Collections.max(fonts.entrySet(),java.util.Map.Entry.comparingByValue()).getKey();
  ReadingText.fontAtlas(swf,swf.getFont(font),out.resolve("font.bin"));ReadingText.paper(swf,speech,out);
  // Speech's subset font omits '+'. Use that actual glyph from the map's
  // embedded font, rather than the old missing-glyph box in the fallback.
  var mapFont=swf.getFont(356);int glyph=mapFont.charToGlyph('+');
  if(glyph<0)throw new IOException("Original map plus sign missing");
  java.awt.geom.Area plus=new java.awt.geom.Area();
  for(var path:com.jpexs.decompiler.flash.exporters.shape.PathExporter.export(
    com.jpexs.decompiler.flash.tags.base.ShapeTag.WIND_EVEN_ODD,1,swf,
    mapFont.getGlyphShapeTable().get(glyph),new java.util.ArrayList<>()))
   plus.add(new java.awt.geom.Area(path));
  var bounds=plus.getBounds2D();if(bounds.isEmpty())throw new IOException("Empty plus outline");
  var image=new java.awt.image.BufferedImage(12,20,java.awt.image.BufferedImage.TYPE_INT_ARGB);
  var g=image.createGraphics();g.setRenderingHint(java.awt.RenderingHints.KEY_ANTIALIASING,java.awt.RenderingHints.VALUE_ANTIALIAS_ON);
  double ratio=Math.min(9/bounds.getWidth(),11/bounds.getHeight());
  g.translate(6,10);g.scale(ratio,ratio);g.translate(-bounds.getCenterX(),-bounds.getCenterY());g.setColor(java.awt.Color.BLACK);g.fill(plus);g.dispose();
  byte[] atlas=Files.readAllBytes(out.resolve("font.bin"));int at=8+('+'-32)*240;
  for(int y=0;y<20;y++)for(int x=0;x<12;x++)atlas[at+y*12+x]=(byte)(image.getRGB(x,y)>>>24);
  Files.write(out.resolve("font.bin"),atlas);
 }
}
