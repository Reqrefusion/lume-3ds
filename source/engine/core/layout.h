#ifndef LUME_ENGINE_LAYOUT_H
#define LUME_ENGINE_LAYOUT_H
/* Media surfaces are RGB24 320x240. Content padding is an application
   profile, not a Flash/game rule. Standalone users default to the full area. */
#ifndef LUME_MEDIA_TOP
#define LUME_MEDIA_TOP 0
#endif
#ifndef LUME_MEDIA_BOTTOM
#define LUME_MEDIA_BOTTOM 240
#endif
#if LUME_MEDIA_TOP < 0 || LUME_MEDIA_BOTTOM > 240 || LUME_MEDIA_TOP >= LUME_MEDIA_BOTTOM
#error Invalid media content rectangle
#endif
#endif
