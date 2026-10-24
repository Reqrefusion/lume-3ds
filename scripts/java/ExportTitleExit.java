import java.io.*;import com.jpexs.decompiler.flash.SWF;import com.jpexs.decompiler.flash.timeline.*;
/* The existing child keeps its clock across gotoAndPlay(animOut). Export only
   the newly placed cover, not a baked copy of a reset forest animation. */
class ExportTitleExit {
 public static void main(String[]a)throws Exception{
  ExportTitleMotion.s=new SWF(new FileInputStream(a[0]),false);ExportTitleMotion.outer=ExportTitleMotion.timeline(1038);ExportTitleMotion.menu=ExportTitleMotion.timeline(170);ExportTitleMotion.anim=ExportTitleMotion.timeline(128);
  for(DepthState d:ExportTitleMotion.outer.getFrame(10).layers.values()){if(d.characterId==170)d.time=0;if(d.characterId==173)d.time=0;}
  for(DepthState d:ExportTitleMotion.menu.getFrame(0).layers.values())if("menuAnim".equals(d.instanceName))ExportTitleMotion.menuAnim=d;
  ExportTitleMotion.menu.getFrame(0).layers.entrySet().removeIf(e->e.getValue()!=ExportTitleMotion.menuAnim);
  for(int f=733;f<776;f++)ExportTitleMotion.anim.getFrame(f).layers.entrySet().removeIf(e->e.getValue().characterId!=64&&e.getValue().characterId!=61);
  ExportTitleMotion.export(2,43,"exit");
 }
}
