#include "storage.h"
#ifndef LUME_MEDIA_TOP
#define LUME_MEDIA_TOP 13
#define LUME_MEDIA_BOTTOM 226
#endif
#include "audio.h"
#include "../../engine/core/log_io.h"
#include "../../platform/3ds/audio_worker.h"
#include "title_animation.h"
#include "../../platform/3ds/math.h"
#include <3ds.h>
#include <mpg123.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#define FRAMES 8192
#define MUSIC_BUFFERS 6
static bool ready, mpg_ready;
static bool music_fading;
static uint64_t music_fade_start;
static unsigned music_fade_level;
static unsigned user_music=100,user_effects=100,authored_music=100;
static mpg123_handle *music;
#define EFFECT_VOICES 8
#define EFFECT_CHANNEL 9
static ndspWaveBuf stream[MUSIC_BUFFERS], notes[8], effects[EFFECT_VOICES];
typedef struct { unsigned id; void *data; unsigned samples,channels; } EffectSample;
static EffectSample samples[] = {
  {.id = 1}, {.id = 2}, {.id = 3}, {.id = 4}, {.id = 5}, {.id = 7}, {.id = 8}, {.id = 9}, {.id = 10},
  {.id = 68}, {.id = 69}, {.id = 72}, {.id = 231}, {.id = 497}, {.id = 500}, {.id = 627}, {.id = 640}, {.id = 655},
  {.id = 774}, {.id = 775}, {.id = 776}, {.id = 777}, {.id = 811}, {.id = 812}, {.id = 831}, {.id = 837},
  {.id = 839}, {.id = 840}, {.id = 841}, {.id = 853}, {.id = 854}, {.id = 860}, {.id = 861}, {.id = 862}, {.id = 898}, {.id = 899}
};
static unsigned voice_cursor, effects_played;
static void flush_audio(const void *data,u32 bytes) {
  /* Avoid a DSP-service IPC round trip on the audio worker. The kernel flush
     accepts our virtual linear address; retain the service path on failure. */
  if(R_FAILED(svcFlushProcessDataCache(CUR_PROCESS_HANDLE,(u32)(uintptr_t)data,bytes)))
    DSP_FlushDataCache(data,bytes);
}
#include "audio_events.h"
static void load_effects(void) {
  unsigned loaded = 0, bytes = 0;
  for (unsigned i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
    char path[64];
    snprintf(path, sizeof(path), LUME_DATA_FILE("sfx_%04u.pcm"), samples[i].id);
    FILE *f = fopen(path, "rb");
    if (!f) { lume_log("SFX missing: %s", path); continue; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);
    if (n > 0 && n <= 4 * 1024 * 1024 && !(n & 1)) {
      void *data = linearAlloc(n);
      if (data && fread(data, 1, n, f) == (size_t)n) {
        samples[i].data = data;
        samples[i].samples = n / 2;
        flush_audio(data, n);
        loaded++;
        bytes += n;
      } else if (data) { linearFree(data); }
    }
    fclose(f);
  }
  lume_log("SFX cache: %u/%u samples, %u bytes, %u voices", loaded,
           (unsigned)(sizeof(samples) / sizeof(samples[0])), bytes, EFFECT_VOICES);
}
static void *music_data[MUSIC_BUFFERS], *note_data[8];
static char music_path[120];
static Thread music_worker;
static LightLock music_lock;
static bool stop_worker;
static int music_request = -1;
static aptHookCookie music_apt_hook;
static bool music_hooked, music_paused, music_parked;
/* Keep compressed music in ordinary RAM. Decoder reads and loop seeks then
   never compete with RomFS video reads or SD save/log writes. PCM handed to
   DSP remains in linear memory. A failed allocation retains file playback. */
static struct { unsigned char *data; size_t size, position; } music_file;
static ssize_t music_read_ram(void *context, void *buffer, size_t count) {
  (void)context;
  size_t remaining=music_file.size-music_file.position;
  if(count>remaining)count=remaining;
  memcpy(buffer,music_file.data+music_file.position,count);
  music_file.position+=count;
  return (ssize_t)count;
}
static off_t music_seek_ram(void *context, off_t offset, int whence) {
  (void)context;
  off_t base=whence==SEEK_SET?0:whence==SEEK_CUR?(off_t)music_file.position:
             whence==SEEK_END?(off_t)music_file.size:-1;
  if(base<0||offset < -base||offset > (off_t)music_file.size-base){errno=EINVAL;return -1;}
  music_file.position=(size_t)(base+offset);
  return (off_t)music_file.position;
}
static void music_drop_ram(void) {
  free(music_file.data);memset(&music_file,0,sizeof music_file);
}
static bool music_load_ram(const char *path) {
  FILE *f=fopen(path,"rb");if(!f)return false;
  bool ok=fseek(f,0,SEEK_END)==0;long length=ok?ftell(f):-1;
  ok=length>0&&length<=16*1024*1024&&fseek(f,0,SEEK_SET)==0;
  if(ok){
    music_file.data=malloc((size_t)length);
    ok=music_file.data&&fread(music_file.data,1,(size_t)length,f)==(size_t)length;
    if(ok)music_file.size=(size_t)length;
  }
  fclose(f);if(!ok)music_drop_ram();return ok;
}
/* APT callbacks never open files, call DSP, join a thread, or hold a lock
   while waiting. The worker acknowledges only after its current refill. */
static void music_apt_event(APT_HookType event,void *unused) {
  (void)unused;
  if(event!=APTHOOK_ONSUSPEND&&event!=APTHOOK_ONRESTORE)return;
  bool pause=event==APTHOOK_ONSUSPEND;
  LightLock_Lock(&music_lock);music_paused=pause;LightLock_Unlock(&music_lock);
  if(pause&&music_worker){
    u64 started=osGetTime();bool parked=false;
    do {
      LightLock_Lock(&music_lock);parked=music_parked;LightLock_Unlock(&music_lock);
      if(parked)break;
      svcSleepThread(1000000);
    }while(osGetTime()-started<200);
    lume_log("AUDIO_HOME suspend parked=%d",parked);
  }else if(!pause)lume_log("AUDIO_HOME restored");
}

static void music_open(bool ending);
static void music_fill(void);
static void music_step(void) {
  LightLock_Lock(&music_lock);
  int request = music_request;
  music_request = -1;
  LightLock_Unlock(&music_lock);
  if (request >= 0)
    music_open(request != 0);
  music_fill();
}
static void music_entry(void *arg) {
  /* FPSCR is per-thread; initialise FZ/DN before the codec's float work. */
  if (!vfp_set_fz_dn())
    return;
  (void)arg;
  for (;;) {
    LightLock_Lock(&music_lock);
    bool stop = stop_worker;
    bool paused = music_paused;
    music_parked = paused;
    LightLock_Unlock(&music_lock);
    if (stop)
      break;
    if(!paused)music_step();
    svcSleepThread(4000000);
  }
}

void lume_log(const char *fmt, ...) {
  va_list args;va_start(args,fmt);lume_log_io_writev(fmt,args);va_end(args);
}
static void channel(int i, int rate, int format) {
  ndspChnSetInterp(i, NDSP_INTERP_LINEAR);
  ndspChnSetRate(i, rate);
  ndspChnSetFormat(i, format);
  float mix[12] = {1, 1};
  ndspChnSetMix(i, mix);
}
bool lume_audio_init(void) {
  Result rc = ndspInit();
  lume_log("ndspInit=%08lx", (unsigned long)rc);
  if (R_FAILED(rc)) {
    s64 id = 0;
    if (R_SUCCEEDED(svcGetSystemInfo(&id, 0x20000, 0)) &&
        (id == 1 || id == 2)) {
      static const unsigned char marker[] = "Lume: Azahar HLE audio interface";
      ndspUseComponent(marker, sizeof(marker), 0xff, 0xff);
      rc = ndspInit();
      lume_log("Azahar HLE ndspInit=%08lx", (unsigned long)rc);
    }
  }
  if (R_FAILED(rc))
    return false;
  ready = true;
  ndspSetOutputMode(NDSP_OUTPUT_STEREO);
  for (int i = 0; i < 8; i++) {
    char path[64];
    snprintf(path, sizeof(path), LUME_DATA_FILE("piano%d.pcm"), i + 1);
    FILE *f = fopen(path, "rb");
    if (!f)
      continue;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);
    if (n <= 0 || n > 1024 * 1024 || (n&1)) {
      fclose(f);
      continue;
    }
    note_data[i] = linearAlloc(n);
    if (note_data[i] && fread(note_data[i], 1, n, f) == (size_t)n) {
      notes[i].data_vaddr = note_data[i];
      notes[i].nsamples = n / 2;
      flush_audio(note_data[i], n);
      channel(i, 22050, NDSP_FORMAT_MONO_PCM16);
    } else if(note_data[i]) {
      /* A short SD/RomFS read is not a playable sample. */
      linearFree(note_data[i]);note_data[i]=NULL;
    }
    fclose(f);
  }
  mpg_ready = mpg123_init() == MPG123_OK;
  channel(8, 44100, NDSP_FORMAT_STEREO_PCM16);
  for (int i = 0; i < EFFECT_VOICES; i++)
    channel(EFFECT_CHANNEL + i, 22050, NDSP_FORMAT_MONO_PCM16);
  load_effects();
  load_authored_events();
  for (int i = 0; i < MUSIC_BUFFERS; i++) {
    music_data[i] = linearAlloc(FRAMES * 4);
    stream[i].data_vaddr = music_data[i];
  }
  LightLock_Init(&music_lock);
  stop_worker=music_paused=music_parked=false;
  int worker_core=-1;
  if(mpg_ready)music_worker=lume_music_worker(music_entry,&worker_core);
  lume_log("Music worker core %d: %s",worker_core,
           music_worker ? "enabled" : "main-thread fallback");
  aptHook(&music_apt_hook,music_apt_event,NULL);music_hooked=true;
  return true;
}
static void music_open(bool ending) {
  if (!ready || !mpg_ready)
    return;
  ndspChnWaveBufClear(8);
  if (music) {
    mpg123_close(music);
    mpg123_delete(music);
    music = NULL;
  }
  music_drop_ram();
  snprintf(music_path, sizeof(music_path), LUME_DATA_FILE("media_%s.mp3"),
           ending ? "0946" : "0006");
  int err;
  music = mpg123_new(NULL, &err);
  if (!music)
    return;
  mpg123_format_none(music);
  mpg123_format(music, 44100, MPG123_STEREO, MPG123_ENC_SIGNED_16);
  bool resident=music_load_ram(music_path);
  int opened;
  if(resident){
    opened=mpg123_replace_reader_handle(music,music_read_ram,music_seek_ram,NULL);
    if(opened==MPG123_OK)opened=mpg123_open_handle(music,&music_file);
  }else opened=mpg123_open(music,music_path);
  if (opened != MPG123_OK) {
    lume_log("Music open failed: %s", music_path);
    mpg123_delete(music);
    music = NULL;
    music_drop_ram();
    return;
  }
  for (int i = 0; i < MUSIC_BUFFERS; i++)
    stream[i].status = NDSP_WBUF_FREE;
  lume_log("Music started: %s ram=%d bytes=%u pcm_buffers=%d", music_path,resident,(unsigned)music_file.size,MUSIC_BUFFERS);
}
static void music_fill(void) {
  if (!ready || !music)
    return;
  for (int i = 0; i < MUSIC_BUFFERS; i++)
    if (music_data[i] && (stream[i].status == NDSP_WBUF_FREE ||
                          stream[i].status == NDSP_WBUF_DONE)) {
      size_t done = 0;
      int rc = mpg123_read(music, music_data[i], FRAMES * 4, &done);
      if (rc == MPG123_NEW_FORMAT && !done)
        rc = mpg123_read(music, music_data[i], FRAMES * 4, &done);
      if (rc == MPG123_DONE && !done) {
        mpg123_seek(music, 0, SEEK_SET);
        rc = mpg123_read(music, music_data[i], FRAMES * 4, &done);
      }
      if (done) {
        stream[i].nsamples = done / 4;
        stream[i].status = NDSP_WBUF_FREE;
        flush_audio(music_data[i], done);
        ndspChnWaveBufAdd(8, &stream[i]);
      }
    }
}
void lume_audio_music(bool ending) {
  if (!ready || !mpg_ready)
    return;
  LightLock_Lock(&music_lock);
  music_request = ending ? 1 : 0;
  LightLock_Unlock(&music_lock);
}
void lume_audio_music_fade_in(void) {
  music_fade_start=osGetTime();music_fade_level=0;music_fading=true;
  lume_audio_music_volume(0);
}
void lume_audio_tick(void) {
  if(music_fading) {
    unsigned level=lume_title_music_percent(osGetTime()-music_fade_start);
    if(level!=music_fade_level){lume_audio_music_volume(level);music_fade_level=level;}
    if(level==100)music_fading=false;
  }
  if (ready && !music_worker && !music_paused)
    music_step();
}
/* Set only the music channel; mute remains a separate master switch and
   effects retain their authored levels during the menu score fade. */
void lume_audio_music_volume(unsigned percent) {
  authored_music=percent>100?100:percent;
  if(ready) {
    float gain=authored_music*user_music/10000.f;
    float mix[12]={gain,gain,0,0,0,0,0,0,0,0,0,0};
    ndspChnSetMix(8,mix);
  }
}
/* User preferences multiply the authored score fade, rather than replacing
   it. Piano and every pooled effect voice share the independent SFX gain. */
void lume_audio_levels(unsigned music_level,unsigned effects_level) {
  user_music=music_level>100?100:music_level;user_effects=effects_level>100?100:effects_level;
  lume_audio_music_volume(authored_music);
  if(ready){float gain=user_effects/100.f;float mix[12]={gain,gain};
    for(int i=0;i<8;i++)ndspChnSetMix(i,mix);
    for(int i=0;i<EFFECT_VOICES;i++)ndspChnSetMix(EFFECT_CHANNEL+i,mix);
  }
}
void lume_audio_mute(bool muted) {
  if (ready)
    ndspSetMasterVol(muted ? 0.f : 1.f);
}
void lume_audio_note(int note) {
  int i = note - 1;
  if (!ready || i < 0 || i >= 8 || !note_data[i])
    return;
  ndspChnWaveBufClear(i);
  notes[i].status = NDSP_WBUF_FREE;
  ndspChnWaveBufAdd(i, &notes[i]);
}
void lume_audio_effect(unsigned id) {
  if (!ready || !id)
    return;
  const EffectSample *sample = NULL;
  for (unsigned i = 0; i < sizeof(samples) / sizeof(samples[0]); i++)
    if (samples[i].id == id && samples[i].data) {
      sample = &samples[i];
      break;
    }
  if(!sample)for(unsigned i=0;i<authored_count;i++)
    if(authored_samples[i].id==id){sample=&authored_samples[i];break;}
  if (!sample)
    return;
  unsigned v = voice_cursor;
  for (unsigned i = 0; i < EFFECT_VOICES; i++) {
    unsigned candidate = (voice_cursor + i) % EFFECT_VOICES;
    if (effects[candidate].status == NDSP_WBUF_FREE ||
        effects[candidate].status == NDSP_WBUF_DONE) {
      v = candidate;
      break;
    }
  }
  ndspChnWaveBufClear(EFFECT_CHANNEL + v);
  memset(&effects[v], 0, sizeof(effects[v]));
  effects[v].data_vaddr = sample->data;
  effects[v].nsamples = sample->samples;
  ndspChnSetFormat(EFFECT_CHANNEL+v,sample->channels==2?NDSP_FORMAT_STEREO_PCM16:NDSP_FORMAT_MONO_PCM16);
  ndspChnWaveBufAdd(EFFECT_CHANNEL + v, &effects[v]);
  voice_cursor = (v + 1) % EFFECT_VOICES;
  effects_played++;
}
void lume_audio_quit(void) {
  if(music_hooked){aptUnhook(&music_apt_hook);music_hooked=false;}
  if (music_worker) {
    LightLock_Lock(&music_lock);
    stop_worker = true;
    LightLock_Unlock(&music_lock);
    threadJoin(music_worker, U64_MAX);
    threadFree(music_worker);
    music_worker = NULL;
  }

  if (music) {
    mpg123_close(music);
    mpg123_delete(music);
    music = NULL;
  }
  music_drop_ram();
  if (ready) {
    for (int i = 0; i < EFFECT_CHANNEL + EFFECT_VOICES; i++)
      ndspChnWaveBufClear(i);
    ndspExit();
  }
  for (int i = 0; i < 8; i++)
    if (note_data[i]) {linearFree(note_data[i]);note_data[i]=NULL;}
  for (int i = 0; i < MUSIC_BUFFERS; i++)
    if (music_data[i]) {linearFree(music_data[i]);music_data[i]=NULL;}
  free_authored_events();
  for (unsigned i = 0; i < sizeof(samples) / sizeof(samples[0]); i++)
    if (samples[i].data) {linearFree(samples[i].data);samples[i].data=NULL;}
  lume_log("SFX queued: %u", effects_played);
  if (mpg_ready)
    mpg123_exit();
  ready=mpg_ready=music_fading=false;music_request=-1;
  memset(stream,0,sizeof stream);memset(notes,0,sizeof notes);memset(effects,0,sizeof effects);
}
