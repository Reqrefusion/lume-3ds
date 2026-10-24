# Shared runtime snapshot

Canonical reusable C code lives in `source/engine`; optional adapters and features
are separate modules. `source/engine/tools` contains the shared model estimator, relief
filter and prepared-animation writer. No original game media or model weights
are included. The snapshot is shared with the Lume and Samorost 1 repositories;
`source-manifest.json` identifies its exact source files.

`source/engine/modules.json` lists the native units for each application. The repository's
`source/` directory owns its game behavior and application integration. There are no forwarding source files or duplicate motor subtrees.

The native sprite adapter serves Samorost timelines; the Flash/media adapter
serves Lume's extracted media. These do not run arbitrary ActionScript. Game
rules, file formats and GPU/application integration remain adapter-specific.

See `BAKED_ANIMATION.md` for source-bound prepared updates, memory limits and
fallback. Host CMake consumers can add this directory and link `media_runtime`;
zlib, libjpeg and liblz4 development packages are needed. Existing `lume_` and
`Sm` API names remain to preserve source compatibility.
