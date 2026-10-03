#pragma once

// What the SMPS Inspector tool (Qt backend, Tools menu) needs from the sound engine. Pure C types so C++ can
// include it (Sound.h is a C-only header). Implemented in Sound.c.

#include <stdint.h>

#define SOUND_INSPECT_CHANNELS 11 // Sound.h's channel layout: PSG1-3 + noise (0-3), FM1-6 (4-9), DAC (10)
#define SOUND_INSPECT_DRUMS 95    // DAC note bytes $81-$DF: the whole ParadoxSMPS drum kit, $DF being the SEGA clip

typedef struct {
    uint8_t music_id;   // the song loaded in the music chip set (0 = none)
    uint8_t fm_count;   // from the song header: DAC + FM tracks (0 = no DAC/FM at all)
    uint8_t psg_count;
    uint8_t active[SOUND_INSPECT_CHANNELS];  // the track is running
    uint8_t noise[SOUND_INSPECT_CHANNELS];   // a PSG track driving the noise channel
    uint8_t keyed[SOUND_INSPECT_CHANNELS];   // a note was struck since the last call
    int16_t note[SOUND_INSPECT_CHANNELS];    // note index (0-95, 0 = C) of the latest struck note
    uint32_t drums[(SOUND_INSPECT_DRUMS + 31) / 32]; // bit n: drum note $81+n played since the last call
} SoundInspectData;

// Returns what happened since the last call (notes struck, drums hit) and the current state, then clears the events.
void Sound_InspectTake(SoundInspectData *out);

// Every sound the engine can play, music first, then effects.
typedef struct {
    uint8_t id;        // the sound test value
    uint8_t is_music;
    const char *name;
} SoundInspectEntry;

int Sound_InspectEntryCount(void);
const SoundInspectEntry *Sound_InspectEntryAt(int index);

// Plays a sound by the byte driver (the real game path) or the JSON engine (the sound test's path); stops everything.
void Sound_InspectPlay(int id, int use_json);
void Sound_InspectStop(void);

// Plays one drum on its own through the sound effect output (note $81+index); index < SOUND_INSPECT_DRUMS.
void Sound_InspectPreviewDrum(int index);
