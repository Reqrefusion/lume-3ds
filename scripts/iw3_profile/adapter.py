"""Run inside the separate official iw3 Python environment."""
from pathlib import Path
import argparse,json,sys

def flags(profile):
 p=profile
 out=['--depth-model',p['depth_model'],'--method',p['method'],'--divergence',str(p['divergence']),
      '--convergence',str(p['convergence']),'--foreground-scale',str(p['foreground_scale']),
      '--edge-dilation',str(p['edge_dilation']),'--ema-normalize','--ema-decay',str(p['ema_decay']),
      '--ema-buffer',str(p['ema_buffer']),'--max-fps',str(p['fps']),'--synthetic-view',p['synthetic_view'],
      '--yes','--format','png','--batch-size','1']
 if p['device']!='auto':out+=['--gpu',str(int(p['device']))]
 if p['depth_resolution'] is not None:out+=['--resolution',str(int(p['depth_resolution']))]
 if p['stereo_width'] is not None:out+=['--stereo-width',str(int(p['stereo_width']))]
 return out

def main():
 ap=argparse.ArgumentParser();ap.add_argument('action',choices=['video','images']);ap.add_argument('profile');ap.add_argument('source');ap.add_argument('dest');ap.add_argument('--repo',required=True);ap.add_argument('--expected-frames',type=int);a=ap.parse_args()
 repo=Path(a.repo).resolve();sys.path.insert(0,str(repo))
 from iw3.utils import create_parser,set_state_args,iw3_main
 from iw3.export_config import ExportConfig,FILENAME,IMAGE_TYPE
 p=json.loads(Path(a.profile).read_text(encoding='utf-8'));dest=Path(a.dest).resolve();dest.mkdir(parents=True,exist_ok=True)
 def run(extra):
  args=create_parser().parse_args(flags(p)+extra);iw3_main(set_state_args(args))
 export=dest/'export'
 extra=['-i',str(Path(a.source).resolve()),'-o',str(export),'--export','--export-depth-fit']
 if a.action=='video':
  import av
  from fractions import Fraction
  if a.expected_frames is None or a.expected_frames<1:raise ValueError('Expected video frame count required')
  # This clock is also used by iw3's internal FPSFilter and AVFilterGraph.
  # An external lossless prepare pass makes both clocks agree. Do not add
  # another fps=25 filter here, nor silently accept a partial export.
  with av.open(str(Path(a.source).resolve())) as container:
   stream=container.streams.video[0]
   if min(stream.width,stream.height)<1 or stream.time_base!=Fraction(1,25) or stream.average_rate!=25:
    raise ValueError('iw3 input must be lossless, source-sized 25 FPS video with a 1:25 time base')
   if stream.frames and stream.frames!=a.expected_frames:raise ValueError(f'Ready video frame count mismatch: {stream.frames}/{a.expected_frames}')
 run(extra)
 configs=list(export.rglob(FILENAME))
 if len(configs)!=1:raise ValueError('Expected exactly one iw3 export YAML file')
 original=configs[0];config=ExportConfig.load(str(original));base=original.parent
 if a.action=='video':
  rgb=sorted((base/config.rgb_dir).glob('*.png'));depth=sorted((base/config.depth_dir).glob('*.png'))
  if len(rgb)!=a.expected_frames or len(depth)!=a.expected_frames:
   raise ValueError(f'Incomplete iw3 depth export: RGB {len(rgb)}, depth {len(depth)}, expected {a.expected_frames}')
 # Rendering the exported video as an image sequence avoids a second lossy
 # video codec and preserves the already temporally normalized depth frames.
 config.type=IMAGE_TYPE;config.fps=1;config.audio_file=None
 config.rgb_dir=(base/config.rgb_dir).resolve().relative_to(dest).as_posix();config.depth_dir=(base/config.depth_dir).resolve().relative_to(dest).as_posix()
 render=dest/'render.yml';config.save(str(render))
 run(['-i',str(render),'-o',str(dest/'sbs')])
 images=sorted((dest/'sbs').glob('*.png'))
 if not images:raise ValueError('iw3 stereo PNG uretmedi')
 rgb=sorted((dest/config.rgb_dir).glob('*.png'));depth=sorted((dest/config.depth_dir).glob('*.png'))
 if len(images)!=len(rgb) or len(depth)!=len(rgb):raise ValueError('iw3 RGB/depth/stereo frame counts differ')
 (dest/'iw3-result.json').write_text(json.dumps({'rgb_dir':str((dest/config.rgb_dir).resolve()),'depth_dir':str((dest/config.depth_dir).resolve()),'frames':len(images),'fps':p['fps'] if a.action=='video' else 1,'processing_order':'source-sized RGB -> depth -> source-sized stereo -> 3DS resize in packer','profile':p},indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
