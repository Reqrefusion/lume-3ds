# Reusing the media engine

This is a reusable 320x240 RGB888 stereo media runtime. It excludes Lume scenes, puzzles, fonts, audio and artwork. Existing `lume_` symbols preserve API compatibility. It is not an ActionScript virtual machine; implement your own game rules.

## Standalone example

`examples/stereo_demo.c` generates its own colored image. START exits and the 3D slider controls the eye separation. It needs no game assets or Flash data. Startup GPU checks can select the software fallback. ARM compilation is checked; physical console testing is separate.

Run `python build_3ds.py --sdk C:/devkitPro`. The SDK must include devkitARM, libctru, citro2d/citro3d, libjpeg, LZ4, zlib and tools/bin. Output: `out/stereo_demo/stereo_demo.3dsx`. The exported engine does not depend on the original project location.

## Integration and lifetime

Your entry point provides `main`. Put your own assets in RomFS if needed. Do not free resources while workers or GPU operations still reference them. `lume_movie_close` completes video jobs; call `lume_gpu_video_close` before gfxExit.

File formats currently fix the surface at 320x240. New games use all 240 rows by default. Specify a half-open crop such as `--media-top 13 --media-bottom 226` when required. These definitions apply to every C unit. `runtime-profile.json` documents compile-time defaults; it is not read at runtime. Changing the crop requires recompilation. The top-screen presentation area is currently x=20..379.

GPU `serial` must increase whenever pixels or stereo content changes; reusing a pointer does not identify an unchanged frame. Call `lume_gpu_video_invalidate()` before CPU writes to the top framebuffer. Image memory must remain valid until GPU presentation completes. `lume_runtime_set_flags_directory` selects an optional flag directory; NULL disables file flags. Keep save management in your game layer.

## Host builds

The CMake example needs GCC/Clang plus zlib, libjpeg and LZ4 development libraries. Worker atomics are not adapted for MSVC. `LE_HOST_WORKERS=OFF` selects synchronous execution. `LE_FLASH_ADAPTER=OFF` and `LE_UI_FEATURES=OFF` disable optional layers. Host builds do not execute the 3DS GPU demo. Host and console performance results are not interchangeable.

## Flash adapter

`adapters/flash/timeline.h` reads extracted timelines and sound events; it does not execute AVM1. Connect those events to your own game state. The Flash adapter is optional for games written from scratch.

## Presentation contract

A left image and left framebuffer are required. NULL right image or right framebuffer selects the left eye. A false result requires the caller to fully repaint through its software path. DMA cache failures do not count as successful presentation. A failed backend is not retried every frame; explicit close permits a new initialization attempt. Startup orientation/color checks cover the complete surface independently of the crop. The renderer owns its citro2d/citro3d lifetime; sharing a graphics context with another engine is unsupported.

## RomFS and notices

`romfs_compact.py` shares byte-identical payloads after packaging, preserving names, sizes, codecs, quality and executable/SMDH bytes. Verification completes before publication. The raw intermediate remains under `out`. This tool supports libctru raw RomFS in 3DSX, not CIA/NCCH/IVFC.

PL_MPEG retains its license in the source header. Preserve third_party notices and supply your own game content.
