import java.io.*;import java.nio.file.*;import java.nio.charset.StandardCharsets;import java.util.*;
import com.jpexs.decompiler.flash.SWF;import com.jpexs.decompiler.flash.tags.DefineSpriteTag;import com.jpexs.decompiler.flash.tags.base.TextTag;import com.jpexs.decompiler.flash.timeline.*;
/* Dialogue timing is derived from the actual root display list, not a
   guessed number of seconds. The final web page becomes readable paper. */
class EndingText {
 static String text(SWF s,int id)throws Exception {
  if(!(s.getCharacter(id) instanceof TextTag))throw new IOException("Missing ending text: "+id);
  String v=ReadingText.original((TextTag)s.getCharacter(id),s);
  v=v.replaceAll("(?i)<br\\s*/?>|</p>","\n").replaceAll("<[^>]+>","");
  v=v.replace("&apos;","'").replace("&nbsp;"," ").replace("&quot;","\"").replace("&amp;","&");
  v=v.replace("at:http", "at:\nhttp").replace("/And for more", "/\n\nAnd for more").replace("out:http", "out:\nhttp");
  return v.replaceAll("[ \\t]+"," ").trim();
 }
 public static void main(String[]args)throws Exception {
  SWF s=new SWF(new FileInputStream(args[0]),false);Timeline root=((DefineSpriteTag)s.getCharacter(988)).getTimeline();
  Path out=Path.of("project/romfs/reading");Files.createDirectories(out);StringBuilder cues=new StringBuilder();
  for(int id:new int[]{964,967,968,969,970,971,972}){
   int first=-1,last=-1;
   for(int fr=2537;fr<=4444;fr++)for(DepthState d:root.getFrame(fr).layers.values())if(d.characterId==id){if(first<0)first=fr;last=fr;}
   if(first<0)throw new IOException("Missing ending dialogue placement: "+id);
   String label="ending_"+id;Files.writeString(out.resolve(label+".txt"),text(s,id)+"\n",StandardCharsets.UTF_8);
   cues.append(first-2537).append('\t').append(last-2537).append('\t').append(label).append('\n');
  }
  Files.writeString(Path.of("project/romfs/ending_reading.tsv"),cues,StandardCharsets.US_ASCII);
  Files.writeString(out.resolve("page_ending.txt"),text(s,974)+"\n\n"+text(s,975)+"\n\n"+text(s,980)+"\n",StandardCharsets.UTF_8);
  System.out.println("Ending: seven timed conversations and readable final page");
 }
}
