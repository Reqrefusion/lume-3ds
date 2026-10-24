# Prepared property-update animation

`source/engine/tools/bake_animation.py` and `source/engine/animation/baked.c` implement an
optional source-bound track format. The first consumer is the native sprite
adapter. Prepared tracks accelerate decoding of move-only frames; they do not
flatten the display tree, merge limbs, precompose world matrices, rasterize
animation or execute ActionScript. Scene/game decisions remain in the application.

## Producer policy

Samorost's `scripts/bake_animations.py` selects frames ending in opcode 128 whose
other operations are exclusively opcode 2 (move an existing display-list slot).
No creation, deletion, replacement, naming, sound, labels, callbacks, FPS changes,
randomization, visibility or playback-control operations enter the prepared path.
Empty frame boundaries may be prepared too. The first frame and every structural
or event frame still use the original interpreter if they do not meet this rule.

Each move contains only the authored changed fields, not a complete absolute
pose. External gameplay changes to fields omitted by that move are retained.
Masks 8, 16, 32, 64, 128 represent translation, diagonal affine terms, off-diagonal
affine terms, eight color terms and alpha. Color resets alpha to one exactly as
the interpreter does; color takes precedence over the alpha-only encoding.
There is no interpolation, quantization, frame-rate change or resampling.

## Runtime contract

The generic reader has no game headers or SDK dependencies. Initialize its
`LeBakedAnimation` as `{0}`; `le_baked_load` replaces owned memory, and any failure
leaves it empty. `le_baked_free` is idempotent. Frame and move records are immutable.
All integers and IEEE-754 float32 values are little endian and four-byte aligned.
Current supported consumers are ARM11 and little-endian host regression builds.

The sprite adapter loads `<scene>.lbk` beside the existing `<scene>.spk`, checks
clip/frame pointers against that package and builds bounded direct frame indexes.
It advances the same source cursor and revision after applying ordered partial
moves. Node allocation, RNG consumption, event timing, frame clocks, seeking,
loop reuse and nested sprite scheduling retain the original path. The diagnostic
`SmVm.baked_enabled` flag can disable prepared execution without reloading assets.

Absent, stale, truncated or invalid tracks fall back to the original interpreter.
Source size plus FNV-1a bind the cache to its exact package; a separate FNV-1a
checks payload integrity. These detect accidental cache mismatches and corruption;
they are not cryptographic authentication. Counts, ranges, ordering, masks and
finite floats are validated; adapter-specific clip/frame ranges are checked too.
The payload allocation is limited to 512 KiB, with a separate 128 KiB cap for
frame lookup tables. Allocation failure discards the accelerator. Scene disposal
releases both tables and payload, including partial-load failure paths.

LBK1 header: magic + seven uint32 values: version (1), source fingerprint, source
byte count, frame count, move count, payload fingerprint, reserved (0).
A frame is six uint32 values: clip ID, zero-based frame index, source cursor,
next cursor, first move index, move count. A move is slot, mask and 15 float32
values: x/y, a/d, b/c, eight color terms, alpha. Frames precede moves. See the
shared writer for the JSON input schema. Saving uses atomic file replacement.

## Using it

Samorost's normal `PREPARE.cmd` produces tracks together with the package and
includes them in the content manifest. Existing converted content can be updated
without processing images, audio or depth:

    python scripts/bake_animations.py

Copy the updated `.lbk` files under `out/3ds/samorost1/levels/` to the matching
SD `3ds/samorost1/levels/` directory along with the rebuilt executable. Copying
the whole port `out/3ds` tree also works. The old `.spk` files remain valid and
are not modified by the standalone bake command. `TEST_AZAHAR.cmd` already copies
these files through its existing recursive data copy.

Lume links the shared module as part of the common runtime; its existing raster
media animation format and game rules are unchanged. Other adapters can produce
and consume LBK1 using the shared reader/writer, without importing Samorost tools.
Standalone engine exports include the generic module and production tools.

## Evidence and limits

The current Samorost content has 14,304 eligible frames in seven scenes, with
14,783 prepared moves; per-scene track files stay below 325 KB. Empty Univerzal
has only a header. `tests/baked_animation.py` compares independent prepared and
interpreted VMs over every source timeline, backward seeks, looping, nested ticks
and external edits. It compares active nodes byte-for-byte, RNG, event/name/sound
order and values. Malformed/stale/missing track tests also exercise live fallback.
The existing complete gameplay smoke suite checks all seven exits and outro.
Runtime logs expose loaded `baked_frames` and cumulative `baked_steps` and
`interpreted_steps` so physical hardware can confirm actual use.

This removes bytecode property decoding; it does not reduce GPU draw calls or
triangle counts. Extra SD reads and RAM are deliberate tradeoffs. No FPS or
real-console speedup is claimed without measurements on the target.
