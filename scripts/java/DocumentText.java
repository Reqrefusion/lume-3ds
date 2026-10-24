import com.jpexs.decompiler.flash.SWF;
import com.jpexs.decompiler.flash.tags.base.TextTag;
import java.io.*;import java.nio.file.*;import java.nio.charset.StandardCharsets;
/* Known visible document text only: never recurse into the popup's debug,
   dialogue, invisible puzzle answers or close controls. IDs belong to Lume.swf. */
public class DocumentText {
 static SWF swf;static Path out;
 static String text(int id)throws Exception{return ReadingText.clean(ReadingText.original((TextTag)swf.getCharacter(id),swf));}
 static void save(String name,String value)throws Exception{
  if(value.getBytes(StandardCharsets.UTF_8).length>8191)throw new IOException("Document exceeds reader: "+name);
  Files.writeString(out.resolve("doc_"+name+".txt"),value+"\n",StandardCharsets.UTF_8);
  System.out.println("Document "+name+": "+value.length()+" characters");
 }
 static String tags(int...ids)throws Exception{StringBuilder s=new StringBuilder();for(int id:ids){if(s.length()>0)s.append("\n\n");s.append(text(id));}return s.toString();}
 public static void main(String[]a)throws Exception{
  swf=new SWF(new FileInputStream(a[0]),false);out=Path.of("project/romfs/reading");Files.createDirectories(out);
  // Text358 is an invisible answer hint and351 is the authoring label POPUPS.
  save("letter",tags(357,364,361)+"\n\n"+text(362)+"\n"+text(363)+"\nA\nG\nA");
  // The gramophone prose is baked into the original page bitmap. This is a
  // checked transcription of that bitmap, including its visible diagram labels.
  save("book","The Development of the Gramophone\n\n9#\n\nThe first recording and playback device was called the phonograph (see fig. 2), and its creation is credited to Thomas Edison, who is famed for having only three months of formal education. The design stores an analogue wave mechanically, on a cylinder which, when turned, vibrates a needle. This vibration is then amplified by a horn.\n\nEmil Berliner improved on the design to create the gramophone in 1887. The major improvement was the use of flat records, with a groove spiralling inwards, which enabled mass production.\n\nFig. 2\nneedle\nhorn\nwax cylinder");
  save("book2",tags(638,637)+"\n\n"+text(639)+text(632));
  int[][] ids={{649,651,652,654},{657},{658},{659},{661},{663,662},{666},{669,667},{649,651,652,654},{649,651,652,654},{673}};
  for(int i=0;i<ids.length;i++)save("book3_"+i,tags(ids[i]));
 }
}
