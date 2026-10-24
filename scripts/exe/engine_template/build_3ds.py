"""Build a standalone 3DS application; no Lume installation or assets needed."""
from pathlib import Path
import argparse,os,subprocess,re,json,hashlib,sys
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from romfs_compact import compact_3dsx
UNITS=['source/engine/animation/baked.c','source/engine/core/jobs_state.c','source/engine/assets/file_cache.c','source/engine/render/blit.c','source/engine/render/stereo_video.c','source/engine/codecs/blocks.c','source/engine/codecs/movie.c','source/engine/platform/3ds/video_present.c']
def build(args):
 if not re.fullmatch(r'[a-zA-Z0-9_-]+',args.name):raise ValueError('Invalid output name')
 if not 0<=args.media_top<args.media_bottom<=240:raise ValueError('Invalid media rectangle')
 base=Path(args.sdk).resolve();arm=base/'devkitARM';port=base/'portlibs/3ds';ctru=base/'libctru';ext='.exe' if os.name=='nt' else ''
 gcc=arm/'bin'/('arm-none-eabi-gcc'+ext);pack=base/'tools/bin'/('3dsxtool'+ext);smdh=base/'tools/bin'/('smdhtool'+ext)
 for f in [gcc,pack,smdh,ctru/'default_icon.png',*[port/'lib'/('lib'+n+'.a') for n in ['jpeg','lz4','z']]]:
  if not f.is_file():raise ValueError('Missing SDK dependency: '+str(f))
 for name in ['citro2d','citro3d']:
  if not any((folder/'lib'/('lib'+name+'.a')).is_file() for folder in [port,ctru]):raise ValueError('Missing SDK dependency: lib'+name+'.a')
 main=Path(args.main);main=main if main.is_absolute() else ROOT/main
 if not main.is_file():raise ValueError('Missing application entry: '+str(main))
 rom=Path(args.romfs).resolve() if args.romfs else None
 if rom and not rom.is_dir():raise ValueError('Missing RomFS directory')
 dest=ROOT/'out'/args.name;dest.mkdir(parents=True,exist_ok=True)
 env=dict(os.environ)
 for n in ['GCC_EXEC_PREFIX','COMPILER_PATH','LIBRARY_PATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH']:env.pop(n,None)
 env['PATH']=os.pathsep.join([str(arm/'bin'),str(base/'tools/bin'),env.get('PATH','')])
 def run(cmd):subprocess.run([str(x) for x in cmd],cwd=ROOT,env=env,check=True)
 arch=['-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft']
 flags=arch+['-mword-relocations','-std=gnu11','-O2','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-D__3DS__','-DARM11','-DLUME_MEDIA_TOP='+str(args.media_top),'-DLUME_MEDIA_BOTTOM='+str(args.media_bottom),'-I'+str(ROOT/'source'),'-I'+str(ctru/'include'),'-I'+str(port/'include')]
 objects=[]
 for i,f in enumerate([ROOT/u for u in UNITS]+[main]):
  obj=dest/(str(i)+'.o');run([gcc,*flags,'-c',f,'-o',obj]);objects.append(obj)
 elf=dest/(args.name+'.elf');icon=dest/(args.name+'.smdh');game=dest/(args.name+'.3dsx')
 run([gcc,*arch,'-specs=3dsx.specs','-Wl,--gc-sections',*objects,'-L'+str(port/'lib'),'-L'+str(ctru/'lib'),'-lcitro2d','-lcitro3d','-ljpeg','-llz4','-lz','-lctru','-lm','-o',elf])
 run([smdh,'--create',args.title,'Standalone stereo media application',args.author,ctru/'default_icon.png',icon])
 raw=dest/(args.name+'.raw.3dsx') if rom else game
 run([pack,elf,raw,'--smdh='+str(icon),*(['--romfs='+str(rom)] if rom else [])])
 if rom:compact_3dsx(raw,game)
 if game.read_bytes()[:4]!=b'3DSX':raise ValueError('Invalid 3DSX output')
 (dest/'build.json').write_text(json.dumps({'title':args.title,'media_top':args.media_top,'media_bottom':args.media_bottom,'sha256':hashlib.sha256(game.read_bytes()).hexdigest(),'sources':{str(f.relative_to(ROOT)) if f.is_relative_to(ROOT) else str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in [ROOT/u for u in UNITS]+[main]}},indent=2),encoding='utf-8')
 print('READY:',game)
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--sdk',required=True,help='devkitpro directory containing devkitARM, libctru and portlibs')
 parser.add_argument('--main',default='examples/stereo_demo.c');parser.add_argument('--romfs')
 parser.add_argument('--name',default='stereo_demo');parser.add_argument('--title',default='Stereo Runtime Demo');parser.add_argument('--author',default='Homebrew')
 parser.add_argument('--media-top',type=int,default=0);parser.add_argument('--media-bottom',type=int,default=240)
 build(parser.parse_args())
