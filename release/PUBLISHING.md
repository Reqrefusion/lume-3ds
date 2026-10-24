# Publishing

Use only the clean source tree for the GitHub repository. Do not initialize Git in a production workspace containing the original EXE, private game data or historical work.

The source tree includes adaptation/engine code, production tools, setup tools and third-party notices. Original EXE/SWF files, RomFS media, font glyph data, model weights, SDK binaries, caches, logs and backups are excluded. Public language files contain interface translations with an empty game-dialogue section.

With a verified current game, original EXE and built setup binary, run:

    python release/build_release.py --dest NEW_OUTPUT_DIRECTORY

The output contains:

- Lume3DS-Source.zip: clean source tree for GitHub.
- Lume3DS-SDCard.zip: copy-to-SD homebrew setup and owner-unlocked ready package.
- Lume3DS-Windows.zip: optional Windows setup for the same ready game.
- SHA256SUMS.txt and RELEASE_CHECKS.json.

Attach the ready ZIPs and checksums to a GitHub Release. The source ZIP can also be attached for convenience. Do not upload an unlocked game file or private developer data to the source repository. Packaging does not publish or contact GitHub.

GPL-3.0-only covers the original adaptation and tools, not original Lume content. Preserve COPYRIGHT.md and all third-party notices. The supported original-file check is a local compatibility gate, not a purchase-validation service. It does not grant additional redistribution rights.

Before claiming hardware support has been tested, verify installation, cancellation, recovery and launching on both Old and New 3DS.
