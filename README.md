# Lume 3DS

A stereoscopic Lume adaptation for Old and New Nintendo 3DS, with the reusable media engine, game source, development tools and complete production pipeline.

## What this repository contains

- Game code, puzzle logic, input handling, persistence and original ActionScript bindings.
- A reusable stereo media engine and the 3DS platform layer.
- EXE/SWF extraction, Java artwork/timeline exporters, audio/video conversion and optional depth generation tools.
- Build scripts, regression tests, documentation and release preparation tools.

This is the main development repository. The maps-only restriction applies to the **prepared content distributed to players**, not to the development tools or game source.

## Repository layout

```text
source/       Game, shared engine, platform and Flash adapter code
scripts/      Extraction, production, conversion and regression tools
build/        Developer ARM build and asset validation scripts
release/      Source packaging and prepared geometry export
maps/         Committed 3D geometry with checksums; no original media
docs/         Development, installation status and publishing guides
languages/    Complete Turkish, German and Spanish translations
third_party/  Dependency sources, notices and provenance
romfs/        Locally generated game assets; excluded from Git
resources/    Local original icon; excluded from Git
```

## Development

See [SYSTEM_SETUP.md](docs/SYSTEM_SETUP.md) for prerequisites and GitHub builds, and [BUILDING.md](docs/BUILDING.md) for producing assets from your own original EXE, building the game, reusing the engine and running tests. Original media, SDK binaries, model weights and local build outputs are not committed.

## Play on Nintendo 3DS

On 64-bit Windows, extract the player setup ZIP into a new folder and run **SETUP.cmd**. No development SDK or depth model is required.

Setup looks for your supported original game at:

    C:\Program Files (x86)\Steam\steamapps\common\Lume\Lume.exe

It also checks the setup folder. If no compatible EXE is found, it opens a file picker. Original media is extracted/converted locally and combined with the committed maps. Existing Python/Java/FFmpeg tools are reused when compatible; missing conversion tools are downloaded with integrity checks. The original EXE is never executed or uploaded.

When setup finishes, copy:

    out/3ds/lume.3dsx
    out/3ds/lume/

to:

    SD:/3ds/lume.3dsx
    SD:/3ds/lume/

Setup copies the precompiled app unchanged; players never build or append data to a 3DSX. Artwork, video, audio, settings, saves and runtime logs live in `SD:/3ds/lume/`.

Start Lume from Homebrew Launcher. Read [installation instructions](docs/INSTALLING.md). The first preparation can take several minutes; successful stages are cached for retries. Physical Old/New 3DS testing remains separate from PC setup verification.

## Prepared geometry in Git

The `maps/` directory contains 166 reviewed geometry files, including displacement for 2,363 video frames. No Git LFS or depth-model download is required. `maps/manifest.json` records their sizes and SHA256 hashes.

`player/lume.3dsx` is the fixed application and contains compiled adaptation code and a public-domain fallback font, and loads game content externally from the SD card. Original game font glyphs are prepared locally. `player/runtime.json` records its SHA256 and source identity. It is not a complete game and needs the locally generated content. Developers can rebuild it with `python build/build_player_runtime.py --sdk C:/devkitPro`.

## Development and distribution

The complete development/production tools remain in this repository. `PRODUCE.cmd` retains the full developer pipeline; `SETUP.cmd` uses the prepared-map player path. Both use the owner's original files locally. The player path blocks depth-inference and stereo-matching modules.

Public downloads include the application, code, tools and geometry, without original game video, audio or playable content. Locally converted game media is private and must not be redistributed. The fixed application can be distributed with its GPL source.

## License

Adaptation, engine and original tools: GPL-3.0-only. Original Lume content is outside this license. Read LICENSE, COPYRIGHT.md and third_party notices.

`BUILD.cmd` compiles only the fixed application. `UPDATE_LANGUAGES.cmd` refreshes complete translations after an initial setup without rerunning video or animation production. GitHub Actions can build and package the application through the **Build 3DSX** workflow.
