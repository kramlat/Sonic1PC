#pragma once

// Audio through Qt Multimedia: two QAudioSinks, one for the music chip set and one for the sound
// effects, so each has its own volume and mute (Audio menu). Both are fed once per frame from
// Audio_Update(), matching the game's own 60 Hz frame-driven architecture instead of running a
// separate audio thread on its own schedule.

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void Audio_Init(void);
void Audio_Update(void); // Call once per frame -- generates and queues that frame's audio
void Audio_Quit(void);

// Audio menu: whole-sink mute and volume (0-100).
void QtAudio_SetMusicEnabled(bool enabled);
bool QtAudio_MusicEnabled(void);
void QtAudio_SetMusicVolume(int percent);
int QtAudio_MusicVolume(void);
void QtAudio_SetSfxEnabled(bool enabled);
bool QtAudio_SfxEnabled(void);
void QtAudio_SetSfxVolume(int percent);
int QtAudio_SfxVolume(void);

#ifdef __cplusplus
}
#endif
