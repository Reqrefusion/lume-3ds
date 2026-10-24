# Installation

## Nintendo 3DS (recommended)

Extract Lume3DS-SDCard.zip to the SD card root. The layout is:

```text
SD:/
  3ds/
    lume-setup/
      lume-setup.3dsx
      lume-setup.smdh
      Lume3DS.l3dp
      Lume.exe             <-- add your own original file
    lume/
      lume.3dsx            <-- created by setup
```

Launch Lume Setup in Homebrew Launcher, then press A. Setup verifies the original file and package before opening the ready assets. Press B to cancel during processing. Keep at least 400 MiB free before starting and avoid interrupting SD writes. The same setup binary targets Old and New 3DS; New 3DS can use its faster CPU. Actual setup timing has not been measured on hardware.

After setup completes, return to Homebrew Launcher and start Lume. You may remove the lume-setup folder afterwards. Keep existing game save data: the current game uses `/3ds/lume_puzzles/` for runtime data, retained for compatibility.

Setup refuses to replace an existing `3ds/lume/lume.3dsx`. To reinstall, back up that file and remove only that executable first. Saves are not touched. After an interrupted setup, remove only `3ds/lume-setup/ready.zip.part` and `3ds/lume/lume.3dsx.part` if present, then retry. Never delete the whole game data folder for setup recovery.

## Optional Windows installation

Extract Lume3DS-Windows.zip and run Install.cmd. Select your own original Lume.exe. Copy the generated `sdmc/3ds` folder to the SD card root. The generated `sdmc/developer` folder is optional private development data and does not need to be copied to the console.

## Required original version

Windows Lume.exe, exactly 21,695,462 bytes.

SHA256: `ba7a947b9a501a1723508e388764113e27b85db7bc14f52b613473373cad0089`.

Other editions are not currently accepted. The EXE is read locally, never executed or uploaded. No model, compiler, FFmpeg or 3D generation is needed to play.

## Verification status

The 3DS setup binary compiles with devkitARM. Its common decoder opens the existing package and reproduces the current game exactly in PC tests. Physical console installation testing remains outstanding.
