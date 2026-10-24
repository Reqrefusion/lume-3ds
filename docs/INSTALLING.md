# Player setup

## Requirements

- 64-bit Windows for local media conversion.
- Your own supported original Lume.exe (21,695,462 bytes).
- An Old or New Nintendo 3DS with Homebrew Launcher.

Supported EXE SHA256: `ba7a947b9a501a1723508e388764113e27b85db7bc14f52b613473373cad0089`.

## Steps

1. Extract Lume3DS-Player-Setup.zip into a writable folder.
2. Run SETUP.cmd. It checks `C:/Program Files (x86)/Steam/steamapps/common/Lume/Lume.exe` and the setup folder, then offers a file picker if needed.
3. Wait for conversion of your original media. Ready 3D maps are reused. No depth model, stereo matching, devkitARM or Citro installation is required for players.
4. Copy the entire `out/3ds/` folder to `SD:/3ds/`.
5. Launch Lume from Homebrew Launcher.

```text
SD:/3ds/lume.3dsx          Fixed application; distributed precompiled
SD:/3ds/lume/             Locally converted images, videos, audio and languages
SD:/3ds/lume/save.lum     Save file, created by the game
SD:/3ds/lume/settings.cfg Language, sound and stereo settings
SD:/3ds/lume/runtime.log  Runtime diagnostics
```

The output is `out/3ds/lume.3dsx` plus `out/3ds/lume/`. Setup copies the same application unchanged: it never builds or appends a filesystem to the 3DSX. You can also download Lume3DS-App.zip separately, but you still need to prepare your own original content.

The first conversion can take several minutes. Successful stages are verified and cached. Existing compatible conversion tools are reused; missing tools are downloaded with integrity checks. Detailed diagnostics go to `logs/player-setup.log`. The original EXE is never executed or uploaded. Do not redistribute converted original media.

## Saves and updates

All active game data uses `/3ds/lume/`. No data is read or imported from the former storage directory.

Repeat setup safely into the same PC output folder. Known local save, settings, log and flag files are preserved; previous content is backed up in `out/.setup-backups/`. On the SD card, copy updated content without deleting your existing saves/settings. Setup does not create initial save files.

## Languages

English, Turkish, German and Spanish include menus, dialogue, puzzle hints, letters, books and ending text. For an existing local preparation, run `UPDATE_LANGUAGES.cmd` to regenerate language/UI assets without converting videos or animations. Select the language in the game settings. The selection is saved in `/3ds/lume/settings.cfg`.

## Custom paths

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/setup_player.ps1 -OriginalExe "D:/Games/Lume/Lume.exe" -OutputDirectory "D:/Lume-Output"
```

Conversion runs on the PC; the console loads external prepared content. Direct EXE conversion on the console is not implemented. Prepared disparity is reused exactly; reconstructed right-eye occlusion pixels can differ from earlier neural-rendered reference images. PC verification does not replace physical Old/New 3DS play testing.
