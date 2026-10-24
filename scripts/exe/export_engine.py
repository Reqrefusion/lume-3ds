"""Export actual reusable C units, without game code or build bridges."""
from pathlib import Path
import argparse,hashlib,json,shutil,sys

def export(source,dest):
    source,dest=Path(source).resolve(),Path(dest).resolve()
    if dest==source or dest.is_relative_to(source):
        raise ValueError("Cannot export inside the engine source tree.")
    if not (source/'engine/core/jobs_state.c').is_file():
        raise ValueError('Engine source directory not found: '+str(source))
    if dest.exists():
        raise ValueError('Engine output already exists; refusing to overwrite it.')
    dest.mkdir(parents=True)
    for name in ['engine','platform','features','adapters']:
        shutil.copytree(source/name,dest/name)
    template=Path(__file__).with_name('engine_template')
    for name in ['build_3ds.py','examples','USAGE.md']:
        item=template/name
        if item.is_dir():shutil.copytree(item,dest/name)
        else:shutil.copy2(item,dest/name)
    (dest/'runtime-profile.json').write_text(json.dumps({'surface':[320,240], 'media_top':0,'media_bottom':240,'stereo_strength':[0,256],'game_assets_included':False,'game_rules_included':False},indent=2),encoding='utf-8')
    shutil.copy2(source.parent/'build/romfs_compact.py',dest/'romfs_compact.py')
    notices=source.parent/'third_party'
    if notices.is_dir():shutil.copytree(notices,dest/'third_party/gpu-sdk-notices')
    cmake='''cmake_minimum_required(VERSION 3.16)
project(stereo_media_runtime C)
option(LE_HOST_WORKERS "Use host pthread workers" ON)
option(LE_FLASH_ADAPTER "Build Flash timeline adapter" ON)
option(LE_UI_FEATURES "Build optional UI callbacks" ON)
find_package(ZLIB REQUIRED)
find_package(JPEG REQUIRED)
find_path(LZ4_INCLUDE_DIR lz4.h REQUIRED)
find_library(LZ4_LIBRARY NAMES lz4 REQUIRED)
add_library(media_runtime STATIC
 engine/core/jobs_state.c engine/assets/file_cache.c
 engine/render/blit.c engine/render/stereo_video.c
 engine/codecs/blocks.c engine/codecs/movie.c)
target_compile_features(media_runtime PUBLIC c_std_11)
target_include_directories(media_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${LZ4_INCLUDE_DIR})
target_link_libraries(media_runtime PUBLIC ZLIB::ZLIB JPEG::JPEG ${LZ4_LIBRARY})
if(LE_HOST_WORKERS)
 find_package(Threads REQUIRED)
 target_compile_definitions(media_runtime PUBLIC LUME_HOST_JOBS)
 target_link_libraries(media_runtime PUBLIC Threads::Threads)
endif()
if(LE_FLASH_ADAPTER)
 target_sources(media_runtime PRIVATE adapters/flash/timeline.c)
endif()
if(LE_UI_FEATURES)
 target_sources(media_runtime PRIVATE features/ui/backend.c)
endif()
'''
    (dest/'CMakeLists.txt').write_text(cmake,encoding='utf-8')
    shutil.copy2(source/'engine/README.md',dest/'README.md')
    manifest={p.relative_to(dest).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(dest.rglob("*")) if p.is_file()}
    (dest/"export-manifest.json").write_text(json.dumps(manifest,indent=2),encoding="utf-8")
    return dest

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dest',type=Path,help='New output directory; existing directories are never overwritten')
    args=parser.parse_args()
    try:
        root=Path(__file__).resolve().parents[2]
        print(export(root/'source',args.dest or root/'media-engine'))
    except Exception as exc:
        print('ERROR:',exc)
        sys.exit(1)
