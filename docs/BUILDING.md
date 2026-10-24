# Development

## Commands

Run these commands from the repository folder:

| Task | Command |
| --- | --- |
| Prepare game files from your own EXE | `SETUP.cmd` |
| Update languages after the first setup | `UPDATE_LANGUAGES.cmd` |
| Compile the application only | `BUILD.cmd --sdk C:/devkitPro` |
| Generate new 3D maps and game assets | `PRODUCE.cmd` |

`SETUP.cmd` and `UPDATE_LANGUAGES.cmd` do not compile the application. `BUILD.cmd` does not convert game content. `PRODUCE.cmd` is the full developer pipeline; players use the prepared maps already in Git.

## Required tools

| Task | Requirements |
| --- | --- |
| Player setup | 64-bit Windows and your supported original `Lume.exe` |
| Application build | Python 3.11+ and devkitPro with the 3DS libraries below |
| Full content production | Python 3.12, Java/FFDec, FFmpeg and the depth/stereo dependencies in `scripts/exe/dependencies.py` and `scripts/iw3_profile/` |

Player setup reuses compatible installed tools and prepares missing conversion tools automatically. Its Python packages are NumPy 1.26.4, Pillow 11.0.0, OpenCV headless 4.10.0.84 and LZ4 4.3.3. Java 17 and FFmpeg handle the source artwork and media. Dependency versions and checksums are recorded in the setup scripts.

Supported EXE SHA256:

```text
ba7a947b9a501a1723508e388764113e27b85db7bc14f52b613473373cad0089
```

For a custom EXE or output folder:

```powershell
powershell -File release/setup_player.ps1 -OriginalExe "D:/Games/Lume/Lume.exe" -OutputDirectory "D:/Lume-Output"
```

## Build the application locally

Use an existing [devkitPro installation](https://github.com/devkitPro/installer/releases). In its pacman shell, install only missing packages:

```sh
pacman -S --needed 3ds-dev 3ds-mpg123 3ds-libjpeg-turbo 3ds-lz4 3ds-zlib
```

On Linux, use `dkp-pacman` instead of `pacman`.

```bat
BUILD.cmd --sdk C:/devkitPro
```

Linux equivalent:

```sh
python3 build/build_player_runtime.py --sdk /opt/devkitpro
```

The build updates `player/lume.3dsx`, its `runtime.json` and `out/3ds/lume.3dsx`. It needs no original EXE, game assets, Java or FFmpeg. Rebuild after changing native code so the application and source hashes stay aligned. To use a modified mpg123 library, point the builder at an SDK containing that replacement library and rebuild normally.

## Build on GitHub

There is one workflow: **Build 3DSX**. It runs on pushes and pull requests, or manually through **Actions > Build 3DSX > Run workflow**.

It checks the sources and translations, builds the application in the devkitPro container, and uploads a single **lume.3dsx** artifact. Attach only this file to a GitHub Release. Players download the repository separately for `SETUP.cmd`, conversion tools, maps and corresponding source code.

Open **Actions > Build 3DSX > Run workflow**:

- Leave **Release name** empty to create or update the **latest** prerelease.
- Enter a name such as **beta**, **1.0**, or **Summer build** to publish a named release. There is no required version-number format. Spaces become hyphens in the Git tag; the displayed release title is preserved. Other Git tag rules still apply.

Successful default-branch pushes also update **latest**. A pushed tag publishes a release under that tag. Pull requests and other branch pushes only produce the Actions artifact. Every release attaches only `lume.3dsx`.

The **latest** prerelease is refreshed with the new application and matching commit. Existing named releases are not overwritten; use a new name for another stable release. The workflow uses the built-in GitHub token and does not convert game content or commit files. Pin the container image digest if you need the same SDK on later rebuilds.

To prepare the release file locally, without compiling or converting anything:

```sh
python release/build_release.py --dest ../lume-release
```

Use a new output folder. Its only file will be `lume.3dsx`. Keep original EXE/SWF files, converted game media, SDK files and model weights out of Git and public downloads.

## Translation checks and engine reuse

```sh
python scripts/check_languages.py
python -m unittest discover -s release/tests -v
```

These checks do not convert content. The native locale test needs a host C compiler and is skipped when one is unavailable. English story text comes from the original EXE; the other language packs must cover every key in `languages/text-keys.txt`.

Export the reusable engine with:

```sh
python scripts/exe/export_engine.py --dest ../media-engine
```

See `scripts/exe/engine_template/USAGE.md` for its API and requirements. Physical Old/New 3DS testing is still needed after runtime changes.