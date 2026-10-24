#ifndef LUME_AUDIO_H
#define LUME_AUDIO_H
#include <stdbool.h>
bool lume_audio_init(void);
void lume_audio_tick(void);
void lume_audio_mute(bool muted);
void lume_audio_levels(unsigned music,unsigned effects);
void lume_audio_music(bool ending);
void lume_audio_music_volume(unsigned percent);
void lume_audio_music_fade_in(void);
void lume_audio_note(int note);
void lume_audio_effect(unsigned id);
void lume_audio_quit(void);
#if defined(__GNUC__)
void lume_log(const char *fmt, ...) __attribute__((format(printf,1,2)));
#else
void lume_log(const char *fmt, ...);
#endif
#endif
