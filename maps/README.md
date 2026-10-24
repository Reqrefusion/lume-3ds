# Lume 3DS geometry maps

Prepared disparity/depth maps only. No original video, music, audio, color image, game executable, SWF, font atlas or ActionScript is included.

SDF1: compressed still-image quarter-pixel correspondence. PDP1: puzzle displacement. TSF1: indexed title displacement. LDP1: indexed video displacement without its color payload.

LDP1 uses little-endian <4sHHIII> (magic, width, height, fps, frame count, group limit), frame_count+1 uint32 offsets, and packets <IHBI> (group first frame, group frame count, depth mode, depth byte count) followed by depth bytes. Prediction modes match the LVM1 disparity decoder. Original group registration is retained. No color bytes are stored.

These maps are not an installable game by themselves. SETUP.cmd combines the committed geometry and media-free runtime with media extracted locally from the owner's supported Lume.exe. The player path runs on a Windows PC, reuses these maps without depth inference or stereo matching, and copies the fixed application unchanged beside external content in SD:/3ds/lume/. Direct conversion on a 3DS is not implemented.
