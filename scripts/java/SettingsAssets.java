import com.jpexs.decompiler.flash.SWF;
import com.jpexs.decompiler.flash.tags.DefineSpriteTag;
import com.jpexs.decompiler.flash.tags.base.TextTag;
import java.nio.file.*;import java.io.*;import java.awt.*;import java.awt.image.BufferedImage;import java.text.Normalizer;
/* Extract original Latin glyphs, paper, icons and button fills. Extra Latin
   accents are composed over the source letter, rather than replacing the font. */
public class SettingsAssets {
 static BufferedImage glyph(byte[]base,int cp){
  BufferedImage im=new BufferedImage(12,20,BufferedImage.TYPE_INT_ARGB);String s=new String(Character.toChars(cp));
  String decomposed=Normalizer.normalize(s,Normalizer.Form.NFD);int letter=decomposed.codePointAt(0);
  if(cp==0x131)letter='i';
  boolean latin=letter>=32&&letter<=126;
  Graphics2D g=im.createGraphics();g.setRenderingHint(RenderingHints.KEY_ANTIALIASING,RenderingHints.VALUE_ANTIALIAS_ON);g.setColor(Color.BLACK);
  if(latin){
   BufferedImage src=new BufferedImage(12,20,BufferedImage.TYPE_INT_ARGB);
   int x0=12,y0=20,x1=0,y1=0;
   for(int y=0;y<20;y++)for(int x=0;x<12;x++){
    int alpha=base[8+(letter-32)*240+y*12+x]&255;src.setRGB(x,y,alpha<<24);
    if(alpha>=64){x0=Math.min(x0,x);x1=Math.max(x1,x+1);y0=Math.min(y0,y);y1=Math.max(y1,y+1);}
   }
   if(x1<=x0||y1<=y0)throw new IllegalArgumentException("Source letter unavailable: "+letter);
   // The source i has faint AA pixels between its dot and stem. An empty-row
   // search cannot separate them. Lowercase body begins at row4 in this atlas.
   if(cp==0x131){for(int y=0;y<4;y++)for(int x=0;x<12;x++)src.setRGB(x,y,0);}
   boolean below=decomposed.indexOf('\u0327')>=0,accent=decomposed.length()>1;
   if(accent&&!below&&y0<4){
    // Reserve cap-accent room while keeping the source baseline fixed.
    // drawImage's width/height overload is deliberately not used here: its
    // y offset previously pushed the whole20px cell below its neighbours.
    g.setRenderingHint(RenderingHints.KEY_INTERPOLATION,RenderingHints.VALUE_INTERPOLATION_BILINEAR);
    g.drawImage(src,0,4,12,16,0,y0,12,16,null);
   }else g.drawImage(src,0,0,null);
   double center=(x0+x1-1)/2.0;int middle=(int)Math.round(center);
   g.setStroke(new BasicStroke(1.15f,BasicStroke.CAP_ROUND,BasicStroke.JOIN_ROUND));
   for(int i=1;i<decomposed.length();i++)switch(decomposed.charAt(i)){
    case '\u0308':
     g.fillRect(Math.max(0,middle-3),1,2,2);g.fillRect(middle+1,1,2,2);break;
    case '\u0307':g.fillRect(Math.max(0,middle-1),1,2,2);break;
    case '\u0306':{
     var path=new java.awt.geom.Path2D.Double();path.moveTo(center-2,0.7);path.quadTo(center,3.3,center+2,0.7);g.draw(path);break;}
    case '\u0302':{
     var path=new java.awt.geom.Path2D.Double();double c=Math.max(2,center);path.moveTo(c-2,2);path.lineTo(c,0.4);path.lineTo(c+2,2);g.draw(path);break;}
    case '\u0303':{
     var path=new java.awt.geom.Path2D.Double();path.moveTo(middle-3,2);path.curveTo(middle-1,-1,middle+1,4,middle+3,1);g.draw(path);break;}
    case '\u0301':g.drawLine(middle-1,2,middle+1,0);break;
    case '\u0300':g.drawLine(middle-1,0,middle+1,2);break;
    case '\u0327':{
     // Attach the cedilla to the actual c/s body, not the centre of a cell.
     var path=new java.awt.geom.Path2D.Double();path.moveTo(center,15.4);path.lineTo(center+0.7,17);path.quadTo(center+1.2,18.5,center-1.2,18.7);g.draw(path);break;}
    default:throw new IllegalArgumentException("Unsupported combining accent U+"+Integer.toHexString(decomposed.charAt(i)));
   }
  }else{
   Font f=new Font("Dialog",Font.PLAIN,18);if(!f.canDisplay(cp))throw new IllegalArgumentException("Install a system font for U+"+Integer.toHexString(cp));
   g.setFont(f);var metrics=g.getFontMetrics();double scale=Math.min(1,10.0/metrics.stringWidth(s));g.translate(6,16);g.scale(scale,1);g.drawString(s,-metrics.stringWidth(s)/2,0);
  }g.dispose();return im;
 }
 public static void main(String[]a)throws Exception{
  ReadingText.main(new String[]{a[0]});EndingText.main(new String[]{a[0]});PuzzleHelpText.main(new String[]{a[0]});NativeMenuArt.main(new String[]{a[0]});DocumentText.main(new String[]{a[0]});
  Path reading=Path.of("project/romfs/reading");byte[] base=Files.readAllBytes(reading.resolve("font.bin"));
  int[] codes=Files.readString(Path.of(a[1])).codePoints().filter(c->c>=32).distinct().sorted().toArray();
  if(codes.length<95||codes.length>224)throw new IOException("Font requires95..224 glyphs");
  try(DataOutputStream out=new DataOutputStream(new BufferedOutputStream(Files.newOutputStream(Path.of("project/romfs/locale_font.bin"))))){
   out.writeBytes("LUF1");out.writeInt(Integer.reverseBytes(codes.length));for(int c:codes)out.writeInt(Integer.reverseBytes(c));
   for(int c:codes){if(c>=32&&c<=126){
     boolean ink=c==32;for(int i=0;i<240;i++)ink|=base[8+(c-32)*240+i]!=0;
     if(ink)out.write(base,8+(c-32)*240,240);else{
      var im=new BufferedImage(12,20,BufferedImage.TYPE_INT_ARGB);var g=im.createGraphics();g.setRenderingHint(RenderingHints.KEY_ANTIALIASING,RenderingHints.VALUE_ANTIALIAS_ON);g.setColor(Color.BLACK);g.setFont(new Font("Dialog",Font.PLAIN,18));String v=Character.toString((char)c);int width=g.getFontMetrics().stringWidth(v);double z=Math.min(1,10.0/Math.max(1,width));g.translate(6,16);g.scale(z,1);g.drawString(v,-width/2,0);g.dispose();for(int y=0;y<20;y++)for(int x=0;x<12;x++)out.writeByte(im.getRGB(x,y)>>>24);
     }
    }else{var im=glyph(base,c);for(int y=0;y<20;y++)for(int x=0;x<12;x++)out.writeByte(im.getRGB(x,y)>>>24);}}
  }
  // New language labels stay independent of the original photographic fills.
  SWF swf=new SWF(new FileInputStream(a[0]),false);
  for(var tag:swf.getCharacters(false).values())if(tag instanceof DefineSpriteTag sp)for(var frame:sp.getTimeline().getFrames())frame.layers.entrySet().removeIf(e->swf.getCharacter(e.getValue().characterId) instanceof TextTag);
  Path atlas=Path.of("project/romfs/native_menu/art.bin");byte[] bytes=Files.readAllBytes(atlas);if(bytes.length!=344500)throw new IOException("Incomplete original menu atlas");int at=52+7*64*64*4+200*28*4;
  for(int id:new int[]{1022,1022,1028}){
   var source=NativeMenuArt.source(swf,id,0,false);var image=new BufferedImage(216,80,BufferedImage.TYPE_INT_ARGB);var g=image.createGraphics();g.setRenderingHint(RenderingHints.KEY_INTERPOLATION,RenderingHints.VALUE_INTERPOLATION_BICUBIC);double z=Math.min(216.0/source.getWidth(),80.0/source.getHeight());int w=(int)Math.round(source.getWidth()*z),h=(int)Math.round(source.getHeight()*z);g.drawImage(source,(216-w)/2,(80-h)/2,w,h,null);g.dispose();
   for(int y=0;y<80;y++)for(int x=0;x<216;x++){int p=image.getRGB(x,y),alpha=p>>>24;bytes[at++]=(byte)((p>>16&255)*alpha/255);bytes[at++]=(byte)((p>>8&255)*alpha/255);bytes[at++]=(byte)((p&255)*alpha/255);bytes[at++]=(byte)alpha;}
  }Files.write(atlas,bytes);System.out.println("Language font: "+codes.length+" glyphs; original icons and blank Save/Quit ready");
 }
}
