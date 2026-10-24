# Stereo media runtime

This is a media/runtime layer, not an AVM1 virtual machine or a universal game
engine. It supports the existing 320x240 RGB24 media surfaces, quarter-pixel
stereo fields, LAN crop/delta streams, MPEG groups, 800x600 detail pages and
independent row rendering. An application supplies gameplay, scenes and input.

- `engine/core`: shared auxiliary-worker budget, application paths, clocks.
- `engine/assets`: bounded file/page caches and lossless raster reads.
- `engine/codecs`: LAN/LVM/LVS/block formats, pixel predictors and PL_MPEG.
- `engine/render`: compositing, viewport filtering and stereo geometry.
- `platform/3ds`: libctru service, DMA/cache and thread-affinity integration.
- `adapters/flash`: optional exported timeline/camera-word/SFX2 adapter. It
  plays extracted data; it does not run ActionScript or resolve Lume scenes.

The engine never includes `game/lume` or `features`. In the full application,
`game/lume` owns scene transitions, seven puzzle rules, hotspots, inventory,
original actor poses and media mappings. `features` contains the added reading,
locale, zoom HUD and button presentation, with font/text/path callbacks supplied
by the application. It never includes the game font or scene definitions.
Lume-specific menu/document rules
remain in `game/lume` and call those feature helpers. C units compile directly in their module directories; compatibility entry files have been removed.

Export with `python scripts/exe/export_engine.py --dest NEW_DIRECTORY`. The output contains engine/platform
and optional UI/Flash adapters and CMake, compiling the actual C units. It has
no game logic, game images, original embedded font, sound or translations.
The media C APIs keep their historical `lume_` prefix for compatibility; that
prefix does not introduce a dependency on `LumeGame` or any Lume asset name.

Host build (requires development packages for zlib, libjpeg and liblz4):

    cmake -S media-engine -B media-engine/build
    cmake --build media-engine/build

`LE_HOST_WORKERS=OFF` selects synchronous operation. The Flash adapter can be
omitted with `LE_FLASH_ADAPTER=OFF`; UI state with `LE_UI_FEATURES=OFF`. A 3DS target still needs its devkitARM,
libctru/portlibs toolchain; the host CMake file is not a replacement for that.

Minimal setup:

    #include "engine/core/jobs_state.h"
    #include "engine/codecs/movie.h"
    lume_runtime_set_asset_directory("assets");
    lume_runtime_set_flags_directory(NULL); /* optional application switches */
    LumeMovie movie = {0};
    /* Open a path chosen by the application, own two RGB24 output buffers. */
    if (lume_movie_open(&movie, "assets/camera.lvs")) {
        lume_movie_set_strength(&movie, 128); /* 0..256 */
        /* lume_movie_frame(&movie, frame, left, right); */
        lume_movie_close(&movie);
    }

All submitted worker arguments and source buffers must remain alive and
immutable until `lume_jobs_wait`. The shared auxiliary budget admits only one
heavy task; nested or competing requests return false and run on the caller.
There is no worker queue to deadlock. Keep UI/application policy off workers.
Set resource directories before starting work, not while file reads run.

Standalone rendering defaults to the complete 320x240 surface. Define
`LUME_MEDIA_TOP` and `LUME_MEDIA_BOTTOM` consistently in every compilation unit
for content padded inside that surface. The Lume application build supplies [13,226) consistently to all units,
which is a content profile; the codecs do not enforce a Lume camera count.
These dimensions are existing format constraints, not dynamically arbitrary
resolutions. UI helpers require application-provided skin/font resources and callbacks;
Lume inventory, Skip visibility and document ownership rules are not exported.

Quality: RGB24, stereo fields, RGBA and quantization remain unchanged. DZP3 is
lossless LZ4 plus an exact reversible predictor. PL_MPEG is MIT; its complete
license is retained in `engine/codecs/pl_mpeg.h`. Do not strip that notice.
Physical 3DS service success/FPS must be measured on the target; host API mocks
only validate lifecycle, equality and fallback logic.

Optional UI setup: include `features/ui/backend.h`, call
`lume_ui_set_user_directory(your_save_folder)` and
`lume_ui_set_fallback(your_95x60_alpha_font, your_text_callback)` before UI use.
Menu artwork, 12x20 glyph atlases, reading paper and translation packs follow
the documented binary contracts and are provided by the application. No game
font data is exported. Application control of Skip visibility and document
ownership stays outside the UI helpers. Locale state is shared across units.


Standalone 3DS build and generated-artwork sample are included in the export.
See USAGE.md and build_3ds.py. The GPU path follows the same compile-time
content rectangle as the codecs; standalone defaults do not use a Lume crop.
