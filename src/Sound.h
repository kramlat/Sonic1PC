#pragma once

#include <stdint.h>

#include "Backend/SN76489.h"

// Sound driver state. Modeled on the real SMPS driver's per-channel RAM
// (SMPS_Track in s1.sounddriver.ram.asm) and its 3-slot sound request queue
// (v_soundqueue0-2) -- simplified into one flat channel array per chip set
// rather than the original's separate music/SFX/special-SFX track RAM
// banks sharing one physical chip, since channel-stealing between tiers
// can just as well be done with a priority check at runtime instead of
// mirroring the original's fixed Z80 RAM layout.
//
// Channel counts are deliberately over-provisioned past the real chips'
// hardware limits, on purpose, to safely reproduce two RAM layout quirks
// the original relies on rather than letting them read/write out of bounds:
//
//   - The extra ("7th") FM slot: on real hardware, a Sonic 1 song with no
//     DAC part still has real DAC track data present -- it's just an SMPS
//     jump command that loops the track back to its own start, forever,
//     rather than actually being empty. The RAM this occupies is what a
//     6th *music* FM channel's track data gets unpacked into instead, and
//     that borrowed track plays on the real YM2612's hardware FM channel 6
//     (via a 07/03 register setup) -- the DAC hardware itself outputs
//     nothing in this mode, it's purely the DAC *track's RAM* being reused
//     to hold a second logical track that targets FM6. Tracked here as its
//     own array slot rather than aliasing it onto the DAC channel's memory.
//   - The 4th PSG slot: the same trick, deliberately extended. The
//     SN76489 itself fully supports all 3 tone channels plus the noise
//     channel running simultaneously (enabled by a 07/04 setup, same idea
//     as FM6's 07/03 above) -- it's purely that the original driver never
//     allocated RAM for a 4th concurrent PSG track, not a hardware limit --
//     one that simply doesn't exist here, since a PC has plenty of RAM to
//     spare for a track struct the size of this one. The extra slot just
//     allocates that room, so SMS tracks (which do use tone+noise
//     together) work for modders instead of the two contending over one
//     slot.
#define SOUND_CHANNELS_PSG 4
#define SOUND_CHANNELS_FM  7 // 6 FM channels + DAC (the 6th FM channel's "borrowed" DAC-slot trick is its own array entry, not aliased)
#define SOUND_CHANNELS     (SOUND_CHANNELS_PSG + SOUND_CHANNELS_FM)

#define SOUND_CHANNEL_PSG_BASE 0
#define SOUND_CHANNEL_FM_BASE  SOUND_CHANNELS_PSG
#define SOUND_CHANNEL_DAC      (SOUND_CHANNEL_FM_BASE + 6)

typedef struct {
    uint8_t playback_control;
    uint8_t voice_control;
    uint8_t tempo_divider;
    const uint8_t *data_ptr;
    int8_t transpose;
    uint8_t volume;
    uint8_t ams_fms_pan;   // FM/DAC only
    uint8_t voice_index;   // FM/PSG only
    uint8_t vol_env_index; // PSG only
    uint8_t stack_pointer;
    uint8_t duration_timeout;
    uint8_t saved_duration; // also doubles as SavedDAC on the DAC channel
    uint16_t freq;
    uint8_t note_timeout;
    uint8_t note_timeout_master;
    const uint8_t *modulation_ptr;
    uint8_t modulation_wait;
    uint8_t modulation_speed;
    int8_t modulation_delta;
    uint8_t modulation_steps;
    uint16_t modulation_val;
    int8_t detune;
    uint8_t psg_noise;    // PSG only
    uint8_t feedback_algo; // FM only
    const uint8_t *voice_ptr; // FM SFX only
    uint32_t loop_counters[3];
} SoundChannel;

// Matches v_soundqueue0-2: one pending request per priority tier, each
// overwritten (not FIFO'd) by the next QueueSoundN call before the driver
// consumes it.
#define SOUND_QUEUE_NORMAL  0 // v_soundqueue0 -- QueueSound1
#define SOUND_QUEUE_SPECIAL 1 // v_soundqueue1 -- QueueSound2
#define SOUND_QUEUE_UNUSED  2 // v_soundqueue2 -- QueueSound3 (unused in the original unless FixBugs is on)
#define SOUND_QUEUE_SIZE    3

// One chip set: a full complement of PSG+FM channel state plus its own
// request queue. Real hardware only has one physical YM2612 and one
// physical SN76489 -- music and SFX have to fight over the same 4+7
// channels, which is the entire reason the original driver's priority
// system (v_sndprio) and channel-stealing between music/SFX/special-SFX
// RAM banks exists at all. A PC has no such constraint: nothing stops us
// from emulating a second, completely independent set of chips -- one
// dedicated to music, one dedicated to SFX -- and downmixing their two
// output streams into one at the very end, in software, right before
// handing samples to SDL. Something impossible on a real Genesis, free
// here.
typedef struct {
    SoundChannel channels[SOUND_CHANNELS];
    uint8_t queue[SOUND_QUEUE_SIZE];
    SN76489 psg; // TODO: YM2612 (FM) emulation -- ymfm is vendored (see /ymfm)
                 // but not wired up yet, so only the PSG half of each chip
                 // set actually produces sound so far.
} SoundChipSet;

extern SoundChipSet sound_music; // Dedicated chip set for music
extern SoundChipSet sound_sfx;   // Dedicated chip set for sound effects

// Genesis PSG/FM clock: both chips are fed a divided-down version of the
// same ~53.69MHz (NTSC) master clock the 68000 runs from.
#define SOUND_PSG_CLOCK 3579545

void Sound_Init(void);

// Renders `count` mono samples at `sample_rate`, downmixing sound_music and
// sound_sfx additively into `out` (which is NOT cleared first by this
// function -- callers must zero it, since it's meant to be summed with
// whatever else shares the output buffer).
void Sound_Generate(int32_t *out, uint32_t count, uint32_t sample_rate);
