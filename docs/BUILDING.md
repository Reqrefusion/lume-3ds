# Building and production

## Reuse the ready assets

Use the import/setup/build commands in README.md. They preserve the ready stereo/video/audio content; they do not convert it again. Required libraries: libctru, citro2d, citro3d, mpg123, libjpeg-turbo, LZ4 and zlib. SDK binaries are not bundled. Configure an existing SDK with `python release/setup_sdk.py --sdk C:/devkitPro`.

`python release/build_ready.py` builds the game as `out/lume.3dsx`. It verifies the assets and package but does not replace the complete regression suite.

## Build the console setup

`python release/build_installer.py --sdk C:/devkitPro`

Output: `build/installer/lume-setup.3dsx` and `.smdh`. Setup additionally requires the existing minizip/zlib portlibs. Vendored AES and SHA256 sources, revisions, checksums and licenses are in `third_party/installer/`. The same decoder can be compiled from `release/installer/host.cpp` for cross-implementation tests.

## Optional full production from the EXE

This is for developers and can be slow. Put your own original Lume.exe in the project root. Run `PRODUCE.cmd` or `powershell -File build/produce.ps1 -Stage all`. Dependencies may be downloaded with checksum verification. Python conversion wheels are pinned in `scripts/exe/windows-wheels.json`. Java/FFDec, FFmpeg and optional depth models are used only on this path.

After production, run `python release/ensure_fallback_font.py`, then `python release/build_ready.py`. Importing ready developer data preserves the exact existing font atlas; deriving the fallback from original glyphs can produce different fallback pixels.

## Reuse the engine in another game

`python scripts/exe/export_engine.py --dest ../media-engine` exports the shared engine, platform, UI and Flash timeline layers with a standalone example. Read `scripts/exe/engine_template/USAGE.md`. Game rules, Lume content and an ActionScript virtual machine are not part of the shared engine.

## Tests

Run `python scripts/quality_check.py` for the full regression suite. Some tests require original game data, Java exports and MSVC or a host C compiler. The original ActionScript snapshots used by certain comparison tests are private data and are not committed to this source package.

Run `powershell -File release/tests/test_package.ps1 -WorkDirectory work/new-package-test` for synthetic package authentication tests. `python release/tests/test_native_package.py --original Lume.exe` checks the shared console decoder against the Windows writer. Hardware tests are separate from host/ARM build checks.

## Relinking mpg123

Private developer data contains application object files and the original SMDH. `release/relink.py` can link a replacement mpg123 library against them and imported RomFS without regenerating 3D. See third_party/README.md for upstream source provenance and SDK library hashes.
