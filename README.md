# Lume 3DS

A stereoscopic Lume adaptation for Old and New Nintendo 3DS. You need your own original Windows copy of Lume to prepare the game files.

## Play

1. Download **lume.3dsx** from [Releases](https://github.com/Reqrefusion/lume-3ds/releases).
2. Download this repository using **Code > Download ZIP** and extract it on a Windows PC.
3. Run **SETUP.cmd**. Select your original `Lume.exe` if it is not found automatically.
4. Copy **out/3ds/lume/** to **SD:/3ds/lume/** and the downloaded **lume.3dsx** to **SD:/3ds/lume.3dsx**.
5. Start Lume from Homebrew Launcher.

Setup checks the usual Steam location:

```text
C:/Program Files (x86)/Steam/steamapps/common/Lume/Lume.exe
```

The SD card should contain:

```text
3ds/
  lume.3dsx
  lume/
    ...game files...
```

The application is already compiled. Setup converts your original game files and uses the supplied 3D maps. No 3DS SDK or depth model is required. Compatible installed conversion tools are reused; missing tools are prepared automatically. Your EXE is never executed or uploaded.

Saves, settings and logs are stored in `SD:/3ds/lume/`. Do not delete your saves when updating game files. Setup diagnostics are in `logs/player-setup.log`.

## Updates and languages

- **Application:** replace `SD:/3ds/lume.3dsx` with **lume.3dsx** from Releases.
- **Language files:** run **UPDATE_LANGUAGES.cmd** after the initial setup, then copy the updated `out/3ds/` folder. Videos and animations are not regenerated.
- **Language selection:** choose English, Turkish, German or Spanish in the game settings. All four cover menus and story text.

## Development

[Build and tool setup](docs/BUILDING.md) explains local builds, GitHub Actions, content production and engine reuse. Source code, production tools and prepared 3D maps are included in this repository.

## License

Project code and tools: GPL-3.0-only. Original Lume content is outside this license. See [LICENSE](LICENSE), [COPYRIGHT.md](COPYRIGHT.md) and [third-party notices](third_party/README.md). Do not redistribute converted game media.