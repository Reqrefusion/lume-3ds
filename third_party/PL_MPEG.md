# PL_MPEG attribution and provenance

Author: Dominic Szablewski — https://phoboslab.org
Upstream: https://github.com/phoboslab/pl_mpeg
Revision: c871f2be022ece7ef4f64230b4fb8e1fb9eb6023
Original pl_mpeg.h git blob: b42f53f89f868a697435ab1e15742f370819ffbc
The upstream header carries `SPDX-License-Identifier: MIT` and the author's name; these are retained. Upstream has no separate license file at this revision. The standard MIT permission text is reproduced below with the upstream attribution.

Local changes: fixed-memory buffers are not compacted by `plm_buffer_discard_read_bytes`, preserving the original complete-buffer length for terminal-picture lookahead and avoiding a per-picture memmove; null-allocation checks in `plm_buffer_create_with_memory`, `plm_video_create_with_buffer`, and the video sequence-header frame allocation. Quantization tables and codeword values are unchanged; subsequent local decoder adaptations are described below. The application uses only fixed-memory MPEG-1 video decoding; audio and game events remain in the existing native system. The implementation is instantiated once, from movie.c through mpeg_reader.h.

## MIT License

Copyright (c) Dominic Szablewski

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Application-side adaptation: mpeg_reader.h reuses the upstream integer BT.601 coefficients and clamping for the active photographed rows. Same-group backward seeks use plm_video_rewind. MPEG reference picture decoding remains on the calling thread.


## Seyrek IDCT durumu

Sifir AC satir/sutunlarinda tam esdeger DC yayma kisayolu eklendi. Onceki tam C fonksiyonuna karsi 100.000 blok ve gercek video kareleriyle kontrol edildi. Kaynak katsayilar/yuvarlama degismez. Benchmark sayaclari sadece LUME_MPEG_IDCT_STATS tanimli kontrol derlemesindedir; normal oyunda eklenmez.

## Prefix VLC decoder

The nine immutable video codebooks are also represented by six-bit prefix tables in mpeg_vlc_tables.h, derived from the same upstream MIT codewords. A one-bit root-leaf shortcut and bounded fixed-memory prefix lookup replace repeated single-bit traversal when enough input is available. Longer codes continue in the original tree; streaming and short tails retain the original checked walker. Values, consumed bits and EOF states matched 4,838,400 test cases. LUME_MPEG_DISABLE_FAST_VLC selects the original walker for comparison. The generated tables retain the author's attribution and this MIT notice by reference. No Topos/FFmpeg source was imported.

## Canonical module location

The full decoder and MIT notice now live in source/engine/codecs/pl_mpeg.h. The short root pl_mpeg.h is a compatibility include. Exported engine sources retain the complete decoder notice and generated-codebook attribution. Gameplay, fonts and media are outside the engine export.
