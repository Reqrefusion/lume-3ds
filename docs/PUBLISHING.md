# Publishing

Publish this repository as the complete development project: game source, reusable engine, production/conversion tools, build scripts, tests and English documentation all belong in GitHub.

The **prepared player content** is a separate release asset containing only existing 3D maps. This does not restrict which original development tools can be published.

## Source package

    python release/build_release.py --dest NEW_DIRECTORY --source-only

This exports a clean source tree and Lume3DS-Source.zip, including the committed maps/ and fixed media-free player/lume.3dsx, without requiring local game assets. Third-party notices are retained. Do not commit your original EXE/SWF, rendered artwork, video/audio, font glyph data, private ActionScript snapshots, model weights or SDK binaries. The Build 3DSX workflow prepares reviewable downloads as Actions artifacts; see SYSTEM_SETUP.md.

## Source and prepared maps

Using the prepared geometry already included in this repository:

    python release/build_release.py --dest NEW_DIRECTORY

Output includes Lume3DS-Source.zip, Lume3DS-Player-Setup.zip , Lume3DS-Maps-Only.zip and Lume3DS-App.zip. The player setup ZIP includes the same complete source for license compliance and is entered through SETUP.cmd. The geometry archive contains existing disparity/depth data; it does not contain original media or a playable game. No original EXE, local RomFS or neural generation is needed for release packaging. To refresh maps after private development, use `release/export_maps.py` with your local RomFS, then review the geometry and update maps/ plus its manifest.

The prepared maps contain geometry only. The separate App ZIP contains the fixed, media-free `3ds/lume.3dsx`, which loads owner-converted content from `SD:/3ds/lume/`. Never bundle original media or a complete playable game. Source/player ZIPs include this same fixed app and corresponding source.

The source/tool license is GPL-3.0-only; original Lume content is outside it. Packaging does not commit, push or upload anything automatically.
