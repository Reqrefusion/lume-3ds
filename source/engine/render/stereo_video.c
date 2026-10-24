#include "engine/render/stereo_video.h"
#include <stdbool.h>
#include "engine/render/disparity.h"
#include <string.h>
/* Small colour tables fit the ARM11 cache, unlike a full 256 KiB table. */
static uint32_t high_colour[256], low_colour[256], expanded[320];
static bool ready;
static void init_colours(void) {
  if (ready)
    return;
  for (unsigned i = 0; i < 256; i++) {
    unsigned r = i >> 3, b = i & 31;
    high_colour[i] =
        ((r << 3) | (r >> 2)) | ((((i & 7) << 5) | ((i & 6) >> 1)) << 8);
    low_colour[i] = ((i >> 5) << 10) | (((b << 3) | (b >> 2)) << 16);
  }
  ready = true;
}
/* @hotpath Exact integer colour expansion and bilinear remap in packed lanes.
 */
void lume_decode_stereo565(const uint8_t *src, uint8_t *left, uint8_t *right) {
  init_colours();
  if(!right){
    for(int i=0;i<320*240;i++,src+=2,left+=3){
      uint32_t c=high_colour[src[1]]|low_colour[src[0]];
      left[0]=c;left[1]=c>>8;left[2]=c>>16;
    }
    return;
  }
  const int8_t *depth=(const int8_t *)(src+320*240*2);
  static const int8_t zero_row[320];
  for(int y=0;y<240;y++){
    uint8_t *left_row=left;
    const int8_t *disparity=depth+y*320;
    bool flat=!memcmp(disparity,zero_row,sizeof zero_row);
    /* Expand and warp one row while its colours are still in the ARM11
       data cache. A full-frame 307200-byte temporary forced a second memory
       pass and displaced the source/output; this row uses only 1280 bytes. */
    if(flat){
      for(int x=0;x<320;x++,src+=2,left+=3){
        uint32_t c=high_colour[src[1]]|low_colour[src[0]];
        left[0]=c;left[1]=c>>8;left[2]=c>>16;
      }
      if(right!=left_row)memcpy(right,left_row,320*3);
      right+=320*3;continue;
    }
    for(int x=0;x<320;x++,src+=2,left+=3){
      uint32_t c=high_colour[src[1]]|low_colour[src[0]];expanded[x]=c;
      left[0]=c;left[1]=c>>8;left[2]=c>>16;
    }
    const uint32_t *row=expanded;
    for(int x=0;x<320;x++,disparity++,right+=3){
      int q = x * 4 + *disparity;
      if (q < 0)
        q = 0;
      if (q > 1276)
        q = 1276;
      unsigned sx = (unsigned)q >> 2, f = (unsigned)q & 3;
      uint32_t c = row[sx];
      if (f) {
        uint32_t b = row[sx < 319 ? sx + 1 : sx];
        unsigned inv = 4 - f;
        uint32_t rb =
            (((c & 0xff00ff) * inv + (b & 0xff00ff) * f + 0x20002) >> 2) &
            0xff00ff;
        uint32_t g =
            (((c & 0xff00) * inv + (b & 0xff00) * f + 0x200) >> 2) & 0xff00;
        c = rb | g;
      }
      right[0] = c;
      right[1] = c >> 8;
      right[2] = c >> 16;
    }
  }
}

static unsigned display_strength=256;
unsigned lume_stereo_strength(void){return display_strength;}
void lume_stereo_set_strength(unsigned value){display_strength=value>256?256:value;}
void lume_decode_stereo565_strength(const uint8_t *src,uint8_t *left,uint8_t *right,unsigned strength) {
 if(!right||strength>=256){lume_decode_stereo565(src,left,right);return;}
 lume_decode_stereo565(src,left,NULL);
 lume_puzzle_reproject_strength(left,right,(const int8_t*)(src+153600),strength);
}
