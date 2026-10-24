#include "engine/render/blit.h"
#include "engine/core/layout.h"
#include "engine/render/stereo_video.h"
#include <string.h>
#include <stdbool.h>
/* @hotpath Integer-only, clipped once. Transparent pixels do no framebuffer
 * work. */
void lume_blit_rgba(uint8_t *rgb, const uint8_t *rgba, int w, int h, int x,
                    int y) {
  int x0 = x < 0 ? -x : 0, y0 = y < 0 ? -y : 0, x1 = w, y1 = h;
  if (x + w > 320)
    x1 = 320 - x;
  if (y + h > 240)
    y1 = 240 - y;
  if (x0 >= x1 || y0 >= y1)
    return;
  for (int yy = y0; yy < y1; yy++) {
    const uint8_t *s = rgba + ((size_t)yy * w + x0) * 4;
    uint8_t *d = rgb + ((y + yy) * 320 + x + x0) * 3;
    for (int xx = x0; xx < x1; xx++, s += 4, d += 3) {
      unsigned a = s[3];
      if (!a)
        continue;
      if (a == 255) {
        d[0] = s[0];
        d[1] = s[1];
        d[2] = s[2];
        continue;
      }
      lume_blend_pixel(d,s,a);
    }
  }
}
void lume_blit_rgba_clipped(uint8_t *rgb,const uint8_t *rgba,int w,int h,int x,int y,
                            int left,int top,int right,int bottom) {
 if(left<0)left=0;
 if(top<0)top=0;
 if(right>320)right=320;
 if(bottom>240)bottom=240;
 int x0=left-x,y0=top-y,x1=right-x,y1=bottom-y;
 if(x0<0)x0=0;
 if(y0<0)y0=0;
 if(x1>w)x1=w;
 if(y1>h)y1=h;
 if(x0>=x1||y0>=y1)return;
 for(int yy=y0;yy<y1;yy++) {
  const uint8_t *src=rgba+((size_t)yy*w+x0)*4;
  uint8_t *dst=rgb+((y+yy)*320+x+x0)*3;
  for(int xx=x0;xx<x1;xx++,src+=4,dst+=3) {
   unsigned alpha=src[3];if(!alpha)continue;
   if(alpha==255){dst[0]=src[0];dst[1]=src[1];dst[2]=src[2];}
   else lume_blend_pixel(dst,src,alpha);
  }
 }
}
#include "engine/platform/3ds/present.inc"
/* @hotpath One inverse transform per image, integer scanline stepping per
 * pixel. */
void lume_blit_affine(uint8_t *rgb, const uint8_t *rgba, int w, int h,
                      const float m[6]) {
  float det = m[0] * m[3] - m[1] * m[2];
  if (det > -.000001f && det < .000001f)
    return;
  float ia = m[3] / det, ic = -m[2] / det, ib = -m[1] / det, id = m[0] / det;
  int32_t du = (int32_t)(ia * 65536.f), dv = (int32_t)(ib * 65536.f),
          ru = (int32_t)(ic * 65536.f), rv = (int32_t)(id * 65536.f);
  int32_t ou = (int32_t)((-ia * m[4] - ic * m[5] + .5f * (ia + ic)) * 65536.f),
          ov = (int32_t)((-ib * m[4] - id * m[5] + .5f * (ib + id)) * 65536.f);
  float px[4] = {m[4], m[0] * w + m[4], m[2] * h + m[4],
                 m[0] * w + m[2] * h + m[4]};
  float py[4] = {m[5], m[1] * w + m[5], m[3] * h + m[5],
                 m[1] * w + m[3] * h + m[5]};
  float lowx = px[0], highx = px[0], lowy = py[0], highy = py[0];
  for (int k = 1; k < 4; k++) {
    if (px[k] < lowx)
      lowx = px[k];
    if (px[k] > highx)
      highx = px[k];
    if (py[k] < lowy)
      lowy = py[k];
    if (py[k] > highy)
      highy = py[k];
  }
  int x0 = (int)lowx - 1, x1 = (int)highx + 1, y0 = (int)lowy - 1,
      y1 = (int)highy + 1;
  if (x0 < 0)
    x0 = 0;
  if (y0 < 0)
    y0 = 0;
  if (x1 > 320)
    x1 = 320;
  if (y1 > 240)
    y1 = 240;
  for (int y = y0; y < y1; y++) {
    int32_t u = ou + du * x0 + ru * y, v = ov + dv * x0 + rv * y;
    uint8_t *d = rgb + (y * 320 + x0) * 3;
    for (int x = x0; x < x1; x++, u += du, v += dv, d += 3) {
      int sx = u >> 16, sy = v >> 16;
      if ((unsigned)sx >= (unsigned)w || (unsigned)sy >= (unsigned)h)
        continue;
      const uint8_t *s = rgba + ((size_t)sy * w + sx) * 4;
      unsigned a = s[3];
      if (!a)
        continue;
      if (a == 255) {
        d[0] = s[0];
        d[1] = s[1];
        d[2] = s[2];
        continue;
      }
      lume_blend_pixel(d,s,a);
    }
  }
}

/* A small foreground plane has one source pixel of maximum parallax.
   Interpolate premultiplied colour/alpha, so transparent borders do not
   produce a dark halo as the slider moves between integer positions. */
void lume_blit_rgba_eye(uint8_t *rgb,const uint8_t *rgba,int w,int h,int x,int y,int eye) {
 lume_blit_rgba_parallax(rgb,rgba,w,h,x,y,eye,4);
}
void lume_blit_rgba_parallax(uint8_t *rgb,const uint8_t *rgba,int w,int h,int x,int y,int eye,int quarter_pixels) {
 int offset=eye==2?quarter_pixels*(int)lume_stereo_strength()/4:0;
 int whole=offset/256,part=offset%256;
 if(part<0){part+=256;whole--;}
 x-=whole;
 unsigned t=(unsigned)part;
 if(!t){lume_blit_rgba(rgb,rgba,w,h,x,y);return;}
 int x0=-1,x1=w,y0=0,y1=h;
 if(x0 < -x)x0=-x;
 if(x1 > 320-x)x1=320-x;
 if(y0 < -y)y0=-y;
 if(y1 > 240-y)y1=240-y;
 if(x0>=x1 || y0>=y1)return;
 /* A film dissolve revisits many alpha pairs. Cache the exact reciprocal
    in a small invocation-owned table, with no shared state between eyes. */
 uint16_t divisors[64]={0};uint32_t reciprocals[64];
 for(int yy=y0;yy<y1;yy++) {
  int dy=y+yy;
  for(int xx=x0;xx<x1;xx++) {
   int dx=x+xx;
   const uint8_t *a=xx>=0?rgba+(yy*w+xx)*4:NULL;
   const uint8_t *b=xx+1<w?rgba+(yy*w+xx+1)*4:NULL;
   unsigned aa=a?a[3]*(256-t):0,ab=b?b[3]*t:0,total=aa+ab;
   if(!total)continue;
   unsigned alpha=(total+128)>>8;if(!alpha)continue;
   uint8_t pixel[3];uint8_t *dst=rgb+(dy*320+dx)*3;
   if(aa&&ab&&a[3]==255&&b[3]==255) {
    for(unsigned c=0;c<3;c++)dst[c]=(a[c]*(256-t)+b[c]*t+128)>>8;
    continue;
   }
   if(!aa){lume_blend_pixel(dst,b,alpha);continue;}
   if(!ab){lume_blend_pixel(dst,a,alpha);continue;}
   /* Equal coverage cancels out of the unpremultiplication. Authored
      full-screen dissolves have long runs of equal alpha: keep the exact
      rounding without three software integer divides per output pixel. */
   if(a[3]==b[3]) {
    for(unsigned c=0;c<3;c++)pixel[c]=(a[c]*(256-t)+b[c]*t+128)>>8;
   }else {
    /* floor((2^32-1)/d) gives a quotient at most one below n/d.
       One shared reciprocal plus correction keeps all RGB channels exact.
       The common neighbouring divisor also reuses its reciprocal. */
    unsigned slot=(total^(total>>8))&63;
    if(divisors[slot]!=total){divisors[slot]=(uint16_t)total;reciprocals[slot]=UINT32_MAX/total;}
    unsigned reciprocal=reciprocals[slot];
    for(unsigned c=0;c<3;c++) {
     unsigned n=a[c]*aa+b[c]*ab+total/2;
     unsigned q=(unsigned)(((uint64_t)n*reciprocal)>>32);
     if(n-q*total>=total)q++;
     pixel[c]=(uint8_t)q;
    }
   }
   lume_blend_pixel(dst,pixel,alpha);
  }
 }
}
