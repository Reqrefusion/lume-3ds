#ifndef LUME_MPEG_Y2R_H
#define LUME_MPEG_Y2R_H
#ifdef __3DS__
#include "engine/platform/3ds/dma_cache.h"
#endif
/* Use the platform's colour converter independently of MPEG decoding. RGB24
   is retained: RGB565 would discard gradients before stereo reprojection.
   The first frame is compared with the existing software path on the device;
   a mismatch, unavailable service or timeout permanently selects software
   for this open movie. No game media or palette is changed. */
typedef struct {
#ifdef __3DS__
 uint8_t *bgr;Handle event;bool service,ready;
#endif
 bool attempted,checked,disabled,direct,retried;
 const char *failure;int32_t result;
} LumeY2R;
static inline void lume_y2r_close(LumeY2R *y) {
#ifdef __3DS__
 if(y->service) {
  Y2RU_StopConversion();
  if(y->event)svcCloseHandle(y->event);
  y2rExit();
 }
 if(y->bgr)linearFree(y->bgr);
 y->bgr=NULL;y->service=y->ready=false;y->event=0;
#else
 (void)y;
#endif
}
static inline bool lume_y2r_init(LumeY2R *y) {
 if(y->attempted)return !y->disabled;
 y->attempted=true;y->disabled=true;
#ifdef __3DS__
 if(lume_runtime_flag("compat_y2r.flag")){y->failure="compat_flag";return false;}
 Result rc=y2rInit();
 if(R_FAILED(rc)){y->failure="init";y->result=rc;return false;}
 y->service=true;
 Y2RU_ConversionParams p={0};
 p.input_format=INPUT_YUV420_INDIV_8;p.output_format=OUTPUT_RGB_24;
 p.rotation=ROTATION_NONE;p.block_alignment=BLOCK_LINE;
 p.input_line_width=320;p.input_lines=240;
 p.standard_coefficient=COEFFICIENT_ITU_R_BT_601_SCALING;p.alpha=255;
#define LUME_Y2R_CHECK(call,label) do {Result err=(call);if(R_FAILED(err)){y->failure=(label);y->result=err;goto failed;}}while(0)
 LUME_Y2R_CHECK(Y2RU_SetConversionParams(&p),"params");
 LUME_Y2R_CHECK(Y2RU_SetSpacialDithering(false),"spatial_dither");
 LUME_Y2R_CHECK(Y2RU_SetTemporalDithering(false),"temporal_dither");
 LUME_Y2R_CHECK(Y2RU_SetTransferEndInterrupt(true),"interrupt");
 LUME_Y2R_CHECK(Y2RU_GetTransferEndEvent(&y->event),"event");
 /* Swapping U/V together with the R/B coefficients and offsets gives
    hardware BGR output in the engine's RGB byte order. This is the same
    conversion matrix, not a new colour correction or reduced precision. */
 Y2RU_ColorCoefficients c;
 bool force_copy=lume_runtime_flag("compat_y2r_copy.flag");
 if(!force_copy && R_SUCCEEDED(Y2RU_GetStandardCoefficient(&c,COEFFICIENT_ITU_R_BT_601_SCALING))) {
  uint16_t t=c.r_V;c.r_V=c.b_U;c.b_U=t;
  t=c.g_V;c.g_V=c.g_U;c.g_U=t;
  t=c.r_offset;c.r_offset=c.b_offset;c.b_offset=t;
  y->direct=R_SUCCEEDED(Y2RU_SetCoefficients(&c));
 }
 if(!y->direct) {
  /* Older/incomplete service implementations retain the existing BGR
     staging path, rather than losing hardware conversion altogether. */
  LUME_Y2R_CHECK(Y2RU_SetConversionParams(&p),"copy_params");
  y->bgr=linearAlloc(320*240*3);if(!y->bgr){y->failure="copy_alloc";y->result=-1;goto failed;}
 }
 y->ready=true;y->disabled=false;return true;
failed:
 lume_y2r_close(y);
#undef LUME_Y2R_CHECK
#endif
 return false;
}
static inline bool lume_y2r_frame(LumeY2R *y,plm_frame_t *f,uint8_t *rgb) {
 if(!lume_y2r_init(y))return false;
#ifdef __3DS__
 if(f->y.width!=320 || f->y.height<240 || f->cb.width!=160 || f->cr.width!=160 ||
    f->cb.height<120 || f->cr.height<120){y->failure="geometry";y->result=-1;goto failed;}
retry:
 ;uint8_t *output=y->direct?rgb:y->bgr;
#define LUME_Y2R_CHECK(call,label) do {Result err=(call);if(R_FAILED(err)){y->failure=(label);y->result=err;goto failed;}}while(0)
 LUME_Y2R_CHECK(lume_dma_flush(f->y.data,320*240),"flush_y");
 LUME_Y2R_CHECK(lume_dma_flush(f->cb.data,160*120),"flush_u");
 LUME_Y2R_CHECK(lume_dma_flush(f->cr.data,160*120),"flush_v");
 LUME_Y2R_CHECK(lume_dma_flush(output,320*240*3),"flush_rgb");
 LUME_Y2R_CHECK(Y2RU_SetSendingY(f->y.data,320*240,320,0),"send_y");
 LUME_Y2R_CHECK(Y2RU_SetSendingU(y->direct?f->cr.data:f->cb.data,160*120,160,0),"send_u");
 LUME_Y2R_CHECK(Y2RU_SetSendingV(y->direct?f->cb.data:f->cr.data,160*120,160,0),"send_v");
 LUME_Y2R_CHECK(Y2RU_SetReceiving(output,320*240*3,320*3*8,0),"receive");
 /* The service owns its completion event. Clearing a shared event is not
    required for DMA; an unsupported clear must not disable conversion. The
    receiving status below still guards early/stale event notifications. */
 (void)svcClearEvent(y->event);
 LUME_Y2R_CHECK(Y2RU_StartConversion(),"start");
 uint64_t limit=y->checked?8000000:20000000;
 uint64_t deadline=svcGetSystemTick()+SYSCLOCK_ARM11*limit/1000000000;
 (void)svcWaitSynchronization(y->event,(int64_t)limit);
 for(;;) {
  bool done=false;
  LUME_Y2R_CHECK(Y2RU_IsDoneReceiving(&done),"receiving_status");
  if(done)break;
  if(svcGetSystemTick()>=deadline){y->failure="timeout";y->result=-1;goto failed;}
  svcSleepThread(100000);
 }
 LUME_Y2R_CHECK(lume_dma_invalidate(output,320*240*3),"invalidate_rgb");
 memset(rgb,0,LUME_MEDIA_TOP*320*3);memset(rgb+LUME_MEDIA_BOTTOM*320*3,0,(240-LUME_MEDIA_BOTTOM)*320*3);
 if(!y->direct)for(unsigned i=LUME_MEDIA_TOP*320*3;i<LUME_MEDIA_BOTTOM*320*3;i+=3) {
  rgb[i]=y->bgr[i+2];rgb[i+1]=y->bgr[i+1];rgb[i+2]=y->bgr[i];
 }
 return true;
failed:
 /* Try the established standard-coefficient/staging path once after a
    direct DMA failure. The retry is bounded and never repeats per frame. */
 if(y->ready&&y->direct&&!y->retried&&strcmp(y->failure?y->failure:"","geometry")) {
  y->retried=true;Y2RU_StopConversion();
  Y2RU_ConversionParams p={0};p.input_format=INPUT_YUV420_INDIV_8;p.output_format=OUTPUT_RGB_24;
  p.rotation=ROTATION_NONE;p.block_alignment=BLOCK_LINE;p.input_line_width=320;p.input_lines=240;
  p.standard_coefficient=COEFFICIENT_ITU_R_BT_601_SCALING;p.alpha=255;
  if(R_SUCCEEDED(Y2RU_SetConversionParams(&p))) {
   y->bgr=linearAlloc(320*240*3);
   if(y->bgr){y->direct=false;goto retry;}
  }
 }
 y->disabled=true;lume_y2r_close(y);
#undef LUME_Y2R_CHECK
#else
 (void)f;(void)rgb;
#endif
 return false;
}
#endif
