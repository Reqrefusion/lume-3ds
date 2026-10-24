# Lume 3DS

A stereoscopic Nintendo 3DS adaptation of Lume, targeting Old and New 3DS.
The adaptation, installer and production tools are licensed under **GPL-3.0-only**. Original game content is not included in that license.

## Play

Download **Lume3DS-SDCard.zip** from Releases and extract it to the root of your SD card. Add your own supported original `Lume.exe` to `/3ds/lume-setup/`. Launch **Lume Setup** from Homebrew Launcher and press A. After installation, return to Homebrew Launcher and launch **Lume**.

The existing 3D images, video, audio and other assets are already prepared. **No 3D generation, model download or PC conversion is required.** Setup runs offline and streams small blocks; it never executes or uploads your EXE.

Supported original: Windows Lume.exe, 21,695,462 bytes, SHA256:

    ba7a947b9a501a1723508e388764113e27b85db7bc14f52b613473373cad0089

See [installation instructions](docs/INSTALLING.md). An optional Windows installer is available as **Lume3DS-Windows.zip**. Keep at least 400 MiB free on the SD card before starting setup. Existing game executables and saves are never overwritten by setup.

The setup homebrew has been ARM-compiled and its shared package decoder has been tested on a PC. Physical Old/New 3DS setup testing is still required before calling the release hardware-verified.

## Repository layout

```text
source/          Game, reusable engine, platform and Flash adapter code
scripts/         Production, extraction, conversion and regression tools
build/           Build and RomFS packaging scripts
release/         3DS/Windows setup code and release packaging tools
docs/            Installation, development and publishing guides
languages/       Optional interface translations (game dialogue excluded)
third_party/     Vendored installer code, notices and source provenance
resources/       Local-only original icon after ready-asset import
romfs/           Local-only imported game assets; excluded from Git
```

## Develop using ready assets

1. Open the optional Windows package with your original EXE. This also provides private developer files.
2. Extract the source package into a fresh folder. Install Python 3.12 and use an existing devkitPro/devkitARM SDK with the required 3DS portlibs.
3. Run `python release/import_ready_assets.py --game ".../sdmc/3ds/lume/lume.3dsx" --private ".../sdmc/developer"`.
4. Run `python release/setup_sdk.py --sdk C:/devkitPro`.
5. Run `python release/build_ready.py` or `BUILD.cmd`.

This imports all existing stereo/media assets byte for byte. It does not invoke Java, FFmpeg or a depth model. Output: `out/lume.3dsx`. See [building](docs/BUILDING.md) for SDK requirements, testing, engine reuse and optional production from the original EXE.

## Production tools

`scripts/exe` contains extraction, audio/video conversion, stereo processing and compression. `scripts/java` contains the original timeline/art exporters. `scripts/iw3_profile` contains optional iw3 integration. These are developer tools; players do not run them. SDK binaries, Python wheels, FFDec/FFmpeg and model weights are not committed to the repository.

## Publishing

Commit only this clean source tree. Upload the ready SD-card and optional Windows ZIPs as release assets. See [publishing](docs/PUBLISHING.md). The packaging tool does not upload anything automatically.

## Licenses

Read [LICENSE](LICENSE), [COPYRIGHT.md](COPYRIGHT.md) and [third_party](third_party/README.md). Preserve third-party notices. The source license does not relicense original Lume content. Optional game translations retain their language choices; the repository documentation and tooling use English.
