#ifndef LUME_AUDIO_DEVICE_H
#define LUME_AUDIO_DEVICE_H
#include <3ds.h>

typedef struct {
  Result initial_result;
  bool emulator_retry;
} LumeAudioDeviceStatus;

/* Keep the normal firmware path on physical consoles. Azahar's HLE audio
   interface also accepts a marker component when dspfirm.cdc is absent. */
static inline Result lume_audio_device_init(LumeAudioDeviceStatus *status) {
  Result result = ndspInit();
  status->initial_result = result;
  status->emulator_retry = false;
  if (R_FAILED(result)) {
    s64 emulator = 0;
    if (R_SUCCEEDED(svcGetSystemInfo(&emulator, 0x20000, 0)) &&
        (emulator == 1 || emulator == 2)) {
      static const unsigned char marker[] = "Media runtime: Azahar HLE audio interface";
      ndspUseComponent(marker, sizeof(marker), 0xff, 0xff);
      status->emulator_retry = true;
      result = ndspInit();
    }
  }
  return result;
}
#endif
