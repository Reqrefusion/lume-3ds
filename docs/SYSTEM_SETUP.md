# System setup

## Choose the task

| Task | Entry point | Requirements |
| --- | --- | --- |
| Play using prepared content | Copy `out/3ds/` to the SD root | Old/New 3DS with Homebrew Launcher |
| Convert an owned original EXE using committed maps | `SETUP.cmd` | 64-bit Windows, supported Lume.exe, conversion tools below |
| Refresh translations after an initial setup | `UPDATE_LANGUAGES.cmd` | The same original EXE and existing prepared content; no video or animation conversion |
| Compile only the fixed application | `BUILD.cmd --sdk C:/devkitPro` | Python 3.11+, existing native SDK below |
| Build on GitHub | Actions > Build 3DSX > Run workflow | Repository with Actions enabled; no local SDK |
| Generate new geometry during development | `PRODUCE.cmd` | Full developer pipeline, original game, additional depth/stereo dependencies |

Players never compile a 3DSX. Their setup copies `player/lume.3dsx` unchanged and prepares external content in `out/3ds/lume/`. Native builds write only `player/lume.3dsx`, `player/runtime.json` and `out/3ds/lume.3dsx`; they never run the content converter. The application stores saves, settings and logs directly in `/3ds/lume/`. No old-directory migration runs.

## Native application build

Install the official [devkitPro toolchain](https://github.com/devkitPro/installer/releases) only if it is not already installed. Select the 3DS development components. Use devkitPro's pacman shell to add only missing packages:

```sh
pacman -S --needed 3ds-dev 3ds-mpg123 3ds-libjpeg-turbo 3ds-lz4 3ds-zlib
```

On Linux use `dkp-pacman` for the same command. The `3ds-dev` group supplies devkitARM, libctru, citro2d, citro3d and the 3DS packaging tools. The other packages supply MP3, JPEG, LZ4 and zlib support. Existing packages are reused by `--needed`.

The builder expects these paths below the SDK root:

```text
devkitARM/bin/arm-none-eabi-gcc[.exe]
libctru/include/ and libctru/lib/libctru.a
portlibs/3ds/include/
portlibs/3ds/lib/libcitro2d.a, libcitro3d.a
portlibs/3ds/lib/libmpg123.a, libjpeg.a, liblz4.a, libz.a
tools/bin/3dsxtool[.exe], smdhtool[.exe]
```

From the repository directory on Windows:

```bat
BUILD.cmd --sdk C:/devkitPro
```

The command reuses the installed embedded Python when present, otherwise the system Python launcher. It installs nothing. The equivalent command on any supported host is:

```sh
python3 build/build_player_runtime.py --sdk /opt/devkitpro
```

Only Python's standard library is needed for this build. Java, FFmpeg, NumPy, the original EXE and generated game media are not needed. The SDK is linked into a temporary build directory and is never copied into the repository.

## Original-media conversion on Windows

Use your supported original `Lume.exe`. SETUP checks the Steam installation at `C:/Program Files (x86)/Steam/steamapps/common/Lume/Lume.exe`, the setup folder, then a file picker. It does not execute or upload the EXE.

The converter uses:

- Python 3.12 with NumPy 1.26.4, Pillow 11.0.0, OpenCV headless 4.10.0.84 and LZ4 4.3.3.
- Java 17 or compatible runtime, FFDec and the pinned Eclipse compiler for Java extraction helpers.
- FFmpeg for audio/video conversion.
- The committed `maps/` geometry and precompiled `player/` application.

SETUP reuses compatible installed tools. Missing tools are prepared automatically with the checksums recorded in `scripts/exe/windows-wheels.json`, `scripts/toolchain.py` and the setup bootstrap. The player route does not install or execute depth models, stereo matching or a native 3DS compiler. No Citro library installation is needed for players.

Run `UPDATE_LANGUAGES.cmd` after editing translations when the initial content already exists. This updates the language/UI assets and republishes the external content; video and animation production stages are not selected. It uses the original EXE for source font glyphs. It does not change the compiled application.

The full `PRODUCE.cmd` developer route is separate. It additionally uses the pinned depth/stereo profile and model requirements under `scripts/iw3_profile/` and `scripts/exe/dependencies.py`. It is needed only to regenerate geometry, not to use the maps already in Git.

## GitHub build and downloads

Push the source changes, `resources/`, `languages/`, prepared `maps/` and the `.github/workflows/` files to the repository. The Build 3DSX workflow runs on relevant native-source pushes and pull requests and can also be started manually from Actions. It uses the official [devkitPro devkitARM container](https://github.com/devkitPro/docker/blob/master/devkitarm/Dockerfile), which includes the 3DS SDK and port libraries.

The workflow checks complete translations, builds the fixed app, runs lightweight tests and prepares public ZIPs. It never reads an original EXE or converts game media. Download the `Lume3DS-Downloads` artifact from the completed run:

- `Lume3DS-App.zip`: the application under `3ds/lume.3dsx`.
- `Lume3DS-Player-Setup.zip`: player conversion tools, prepared maps and matching sources.
- `Lume3DS-Source.zip`: the corresponding project source and tools.
- `Lume3DS-Maps-Only.zip`: prepared geometry only.
- `SHA256SUMS.txt`, `runtime.json` and `toolchain-packages.txt`: integrity and build identities.

No secret token, signing key, write permission, automatic commit or automatic release publication is configured. Downloaded artifacts can be reviewed and attached to a release manually. The workflow uses a moving container tag and records package versions; pin its image digest for a release that needs an identical SDK environment on later rebuilds.

## Lightweight checks

```sh
python scripts/check_languages.py
python -m unittest discover -s release/tests -v
```

These checks do not run content conversion or depth generation. A host C compiler enables the native locale test; it is skipped if none is installed. English story text comes from the owned original EXE. Turkish, German and Spanish include all 83 authored text keys, including two intentionally empty labels. The checker rejects missing story/document text, incomplete menus, unsupported accents and font/reader capacity violations.