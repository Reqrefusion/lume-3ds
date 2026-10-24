# Development and production

This repository includes both the game/runtime source and its production tools. Developers can build private game copies from their own original files. Public prepared-content downloads are limited to geometry maps.

## Requirements

Use Python 3.12 and an existing devkitPro/devkitARM SDK with libctru, citro2d, citro3d, mpg123, libjpeg-turbo, LZ4 and zlib. The SDK is not bundled.

Configure an existing SDK:

    python release/setup_sdk.py --sdk C:/devkitPro

## Produce from the original EXE

Place your own supported original Lume.exe in the project root, then run:

    PRODUCE.cmd

or:

    powershell -File build/produce.ps1 -Stage all

The computer-side pipeline extracts original SWF/media, exports artwork and timelines, converts audio/video, generates stereo/depth data and assembles the local RomFS. It may download verified dependencies; Java/FFDec, FFmpeg and optional depth models are development dependencies. Python wheels are pinned in scripts/exe/windows-wheels.json.

This full developer route still performs depth/stereo generation. For player setup, use SETUP.cmd or release/setup_player.py; it supplies --prepared-maps to the producer and blocks inference/matching modules.

## Build a local game

Once local assets exist:

    python release/build_ready.py --sdk C:/devkitPro

Output: out/3ds/lume.3dsx plus external content in out/3ds/lume/. BUILD.cmd builds only the fixed app and does not publish or convert media. The explicit release/build_ready.py entry also republishes existing validated local assets. Keep original font data and romfs out of Git. The code fallback font is public-domain font8x8; the original readable fonts are prepared locally as game assets.

Developers who already have their own private game build and matching developer data can import them with:

    python release/import_ready_assets.py --game PATH_TO_OWN_GAME --private PATH_TO_OWN_DEVELOPER_DATA

This is a private development convenience, not a public player download.

## Reuse the engine

    python scripts/exe/export_engine.py --dest ../media-engine

Read scripts/exe/engine_template/USAGE.md for the standalone example, platform requirements and buffer lifetime contracts. The reusable engine does not execute ActionScript or implement another game's rules.

## Tests

    python scripts/quality_check.py

Some regression tests require original assets, private ActionScript snapshots, Java exports and MSVC or a host C compiler. These private fixtures are not committed. Host tests and ARM compilation do not replace physical Old/New 3DS testing.

Run `python -m unittest discover -s release/tests -v` for geometry isolation, corrupt-input rejection and source export checks. These tests require no original game or SDK.

## Prepare public downloads

    python release/build_release.py --dest NEW_DIRECTORY

This validates the committed maps/ and packages the source repository with its geometry-only content. No original EXE or local RomFS is required for packaging. Use --source-only to package development sources without local RomFS. See docs/PUBLISHING.md. No generated video/audio/art or complete game is bundled.

## Fixed application

Developers run `python build/build_player_runtime.py --sdk C:/devkitPro` after changing native source. This builds the fixed app with public-domain fallback glyphs. The temporary marker filesystem is discarded, and the released `player/lume.3dsx` has no embedded filesystem. Original Lume media and font glyphs are external files, prepared from the player's own original EXE.

`player/runtime.json` records application integrity and native source hashes. Player setup verifies them, then copies the application unchanged to `out/3ds/lume.3dsx` and publishes verified content to `out/3ds/lume/`. Only developers need the native SDK and Citro libraries. Player conversion may compile Java extraction helpers using the pinned Eclipse compiler; it never compiles or assembles a 3DSX.

See [SYSTEM_SETUP.md](SYSTEM_SETUP.md) for the complete tool list, installation commands, language-only updates and GitHub Actions downloads.
