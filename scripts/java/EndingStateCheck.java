import java.nio.file.*;import java.util.*;import com.jpexs.decompiler.flash.timeline.*;
/* Offline regression of the real final-state exporter; no game runtime stubs. */
public class EndingStateCheck {
 public static void main(String[]a)throws Exception {
  SourceGame.input=a[0];SourceGame.requests=Path.of(a[1]);SourceGame.load();var root=SourceGame.t(988);
  SourceGame.readableEnding(root);SourceGame.endingFinalProps(root);int turbines=0,tails=0;
  for(int frame=2537;frame<=4444;frame++)for(DepthState d:root.getFrame(frame).layers.values()){
   if(d.characterId==216&&d.isVisible)throw new IllegalStateException("Solved solar cover reappeared");
   if(d.characterId==208){if(d.time!=41)throw new IllegalStateException("Construction restarted");turbines++;}
   if(d.characterId==963&&d.depth==330)tails++;
  }
  if(turbines!=1908||tails!=587)throw new IllegalStateException("Final-state coverage "+turbines+"/"+tails);
  for(Frame f:SourceGame.t(213).getFrames())for(DepthState d:f.layers.values())
   if("blades".equals(d.instanceName))throw new IllegalStateException("Fence blades reappeared");
  var finalFrame=SourceGame.t(208).getFrame(41);Set<String> parts=new HashSet<>();
  for(DepthState d:finalFrame.layers.values())if(d.instanceName!=null)parts.add(d.instanceName);
  if(!parts.containsAll(Arrays.asList("poles","dynamo","blades")))throw new IllegalStateException("Final turbine incomplete");
  // Isolate the turbine, keeping its actual root camera matrix. Rendering
  // checks that advancing the child clock really changes visible pixels.
  var f=root.getFrame(3217);f.layers.entrySet().removeIf(e->e.getValue().characterId!=208);
  Set<Integer> images=new HashSet<>();for(int i=0;i<48;i++){
   SourceGame.endingRotorFrame(2537+i);
   images.add(Arrays.hashCode(SourceGame.pixels(SourceGame.stage(root,3217,true),false)));
   for(DepthState d:finalFrame.layers.values())if(d.characterId==206&&d.time!=i)throw new IllegalStateException("Rotor clock");
  }
  if(images.size()<24)throw new IllegalStateException("Rotor froze: "+images.size());
  SourceGame.load();root=SourceGame.t(988);SourceGame.introInitialProps(root);int initial=0;
  for(int frame=1751;frame<=2535;frame++)for(DepthState d:root.getFrame(frame).layers.values())
   if(d.characterId==208){if(d.time!=0||d.isVisible)throw new IllegalStateException("Intro undiscovered turbine visible");initial++;}
  boolean fenceBlades=false;for(Frame ff:SourceGame.t(213).getFrames())for(DepthState d:ff.layers.values())
   if("blades".equals(d.instanceName))fenceBlades=true;
  if(initial!=785||!fenceBlades)throw new IllegalStateException("Intro initial state changed");
  System.out.println("PASS intro props: 785 stage1 poses, uncollected fence blades retained");
  System.out.println("PASS ending props: 1908 final turbine poses, 587 speaker tails, fence blades removed, rotor phases="+images.size());
 }
}
