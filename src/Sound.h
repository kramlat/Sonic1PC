#pragma once

#include <stdint.h>

#include "Backend/SN76489.h"
#include "Backend/YM2612.h"

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
    uint8_t note_timeout_master; // persists across notes -- set by smpsNoteFill ($E8)
    const uint8_t *modulation_ptr;
    uint8_t modulation_wait;
    uint8_t modulation_speed;
    int8_t modulation_delta;
    uint8_t modulation_steps;
    int16_t modulation_val;
    int note_index; // last real note-on's pitch (post-transpose), so modulation can recompute frequency each frame without a note retrigger
    int8_t detune;
    uint8_t psg_noise;    // PSG only
    uint8_t fm_base_tl[4]; // FM only -- each operator's TL as loaded from the
                            // voice bank, before ch->volume is added as an
                            // offset (see FM_LoadVoice/FM_ApplyVolume)
    uint8_t feedback_algo; // FM only
    const uint8_t *voice_ptr; // FM SFX only
    uint32_t loop_counters[3];

    // Runtime interpreter state (not modeled on SMPS_Track RAM -- this
    // engine's byte-stream interpreter needs a real return-address stack
    // and explicit active/key-on flags where the original just relied on
    // fixed Z80 RAM layout and unconditional jumps).
    const uint8_t *return_stack[2]; // smpsCall/smpsReturn ($F8/$E3) -- 2 levels deep, more than any real song nests
    uint8_t return_sp;
    uint8_t active;  // 0 once smpsStop/smpsStopSpecial ($F2/$EE) is hit, or channel unused by the loaded song
    uint8_t key_on;  // 1 while gated on (audible); cleared during a note-fill release tail or on a rest
    uint8_t mod_active; // smpsModOn/smpsModOff ($F1/$F4) -- parameters stored above are not yet applied per-frame (TODO)
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
    SN76489 psg;
    struct YM2612 *fm; // Backend/YM2612.h -- one physical chip per chip set, same dual-chip-set idea as psg
    const uint8_t *voice_bank; // Current song's FM voice table (25 bytes/voice) -- see smpsSetvoice/$EF

    // Shared tempo governor -- one per chip set (not per channel; see
    // smpsChanTempoDiv's comment in Sound.c for why per-channel overrides
    // aren't independently timed yet). Matches the real driver's
    // TempoWait/v_main_tempo_timeout mechanism, NOT a simple accumulator:
    // every frame, every channel's duration_timeout decrements by 1
    // unconditionally; separately, tempo_timeout counts down from
    // main_tempo each frame, and when it hits 0 it resets to main_tempo AND
    // adds 1 back to every channel's duration_timeout (a periodic "delay by
    // one extra frame" correction -- "duty cycle" = how many of every
    // main_tempo frames actually advance vs. get delayed). duration_mult
    // is unrelated to that -- it's a flat multiplier applied to a note's
    // raw duration byte the moment it's read (header offset $04,
    // "dividing timing"), not part of the per-frame governor at all.
    uint8_t duration_mult;
    uint8_t main_tempo;
    uint8_t base_main_tempo;   // main_tempo as loaded from the header, cached so SlowDownMusic can restore it
    uint8_t current_music_id;  // Currently-loaded music ID, for SpeedUpMusic's per-song SpeedUpIndex lookup
    uint8_t psg_count;         // Cached from the header, for $F3's "only if psg_count<4" gate
    uint8_t tempo_timeout;

    // DAC/DPCM percussion sample playback (kick/snare/timpani only so far
    // -- see DACSampleID). One sample plays at a time per chip set, matching
    // the real driver's single physical DAC.
    const uint8_t *dac_data;
    uint32_t dac_nibble_pos;
    uint32_t dac_nibble_count;
    uint8_t dac_accum;   // 8-bit DPCM decode accumulator, centered at 0x80
    uint8_t dac_playing;
    uint32_t dac_phase; // fixed-point phase accumulator for dac_rate->output-rate step
    uint32_t dac_rate;  // Hz -- the currently-triggered sample's own real rate (see DACSample::rate)

    // Raw PCM playback (the "SEGA!" boot voice clip, res/PCM/sega) -- unlike
    // the DPCM percussion above, this is plain unsigned 8-bit samples with
    // no header and no delta-decode, played back directly (see
    // PlaySegaSound). Independent of dac_playing/dac_data.
    uint32_t pcm_pos;
    uint8_t pcm_playing;
    uint32_t pcm_phase;

    // DAC/PCM output pan -- real hardware shares this with FM channel 6's
    // own $B4 register (DAC replaces channel 6's audio when DAC_EN is set,
    // but still uses channel 6's stereo bits); this port gives the DAC an
    // independent pan state instead of literally sharing a register with
    // FM6, which is simpler and has the same audible effect. Set via $E0
    // on the DAC track; top 2 bits are the L/R enable bits, same encoding
    // as FM_SetPan. Shared by both DPCM percussion and the raw PCM path
    // (Sega sound), since they're the same physical DAC.
    uint8_t dac_pan;

    uint8_t paused; // Sound_Pause()/Sound_Resume() -- freezes tempo advancement (sound_music only)

    // v_sndprio equivalent -- tracks the priority of whatever's currently
    // occupying sound_sfx's channels, so a lower-priority PlaySound() can't
    // cut it off (matches CycleSoundQueue's ".blo .nextinput" reject). Not
    // meaningful for sound_music (music always just replaces outright).
    uint8_t current_priority;
} SoundChipSet;

extern SoundChipSet sound_music; // Dedicated chip set for music
extern SoundChipSet sound_sfx;   // Dedicated chip set for sound effects

// Genesis PSG/FM clock: both chips are fed a divided-down version of the
// same ~53.69MHz (NTSC) master clock the 68000 runs from.
#define SOUND_PSG_CLOCK 3579545

// The 3 real DPCM percussion samples (res/DAC/*.dpcm). dKick/dSnare/dTimpani
// in smps.h map directly to these. The pitch-shifted timpani variants
// (dHiTimpani..dVLowTimpani) and the "SEGA!" boot voice PCM sample
// (res/PCM/sega.pcm, a different codec entirely -- see zPlay_SegaPCM in the
// real driver) are NOT implemented yet.
#define DAC_SAMPLE_KICK    0
#define DAC_SAMPLE_SNARE   1
#define DAC_SAMPLE_TIMPANI 2

// Sound IDs -- matches the real disasm's bgm_*/sfx_* equates (_Constants.asm)
// exactly, both by name and by value (each one is just the hex byte baked
// into its resource's own filename -- Mus81 = bgm_GHZ = 0x81, etc.).
#define bgm_GHZ        0x81
#define bgm_LZ         0x82
#define bgm_MZ         0x83
#define bgm_SLZ        0x84
#define bgm_SYZ        0x85
#define bgm_SBZ        0x86
#define bgm_Invincible 0x87
#define bgm_ExtraLife  0x88
#define bgm_SS         0x89
#define bgm_Title      0x8A
#define bgm_Ending     0x8B
#define bgm_Boss       0x8C
#define bgm_FZ         0x8D
#define bgm_GotThrough 0x8E
#define bgm_GameOver   0x8F
#define bgm_Continue   0x90
#define bgm_Credits    0x91
#define bgm_Drowning   0x92
#define bgm_Emerald    0x93

#define sfx_Jump         0xA0
#define sfx_Lamppost     0xA1
#define sfx_Death        0xA3
#define sfx_Skid         0xA4
#define sfx_HitSpikes    0xA6
#define sfx_Push         0xA7
#define sfx_SSGoal       0xA8
#define sfx_SSItem       0xA9
#define sfx_Splash       0xAA
#define sfx_HitBoss      0xAC
#define sfx_Bubble       0xAD
#define sfx_Fireball     0xAE
#define sfx_Shield       0xAF
#define sfx_Saw          0xB0
#define sfx_Electric     0xB1
#define sfx_Drown        0xB2
#define sfx_Flamethrower 0xB3
#define sfx_Bumper       0xB4
#define sfx_Ring         0xB5
#define sfx_SpikesMove   0xB6
#define sfx_Rumbling     0xB7
#define sfx_Collapse     0xB9
#define sfx_SSGlass      0xBA
#define sfx_Door         0xBB
#define sfx_Teleport     0xBC
#define sfx_ChainStomp   0xBD
#define sfx_Roll         0xBE
#define sfx_Continue     0xBF
#define sfx_Basaran      0xC0
#define sfx_BreakItem    0xC1
#define sfx_Warning      0xC2
#define sfx_GiantRing    0xC3
#define sfx_Bomb         0xC4
#define sfx_Cash         0xC5
#define sfx_RingLoss     0xC6
#define sfx_ChainRise    0xC7
#define sfx_Burning      0xC8
#define sfx_Bonus        0xC9
#define sfx_EnterSS      0xCA
#define sfx_WallSmash    0xCB
#define sfx_Spring       0xCC
#define sfx_Switch       0xCD
#define sfx_RingLeft     0xCE
#define sfx_Signpost     0xCF
#define sfx_Waterfall    0xD0

#define bgm_Fade     0xE0
#define sfx_Sega     0xE1
#define bgm_Speedup  0xE2
#define bgm_Slowdown 0xE3
#define bgm_Stop     0xE4
#define DAC_SAMPLE_COUNT   3

void Sound_Init(void);

// Queues a sound ID for the driver to pick up on its next Sound_Frame()
// tick -- matches the real QueueSound1 (normal/music) and QueueSound2
// (special/priority SFX). Each overwrites its slot rather than FIFO'ing.
// Low-level primitives -- PlayMusic()/PlaySound() below are what game code
// should actually call; they route through these.
void QueueSound1(uint8_t id);
void QueueSound2(uint8_t id);

// High-level entry points -- matches the real PlaySoundID's dispatch by ID
// range (bgm__First-Last $81-$93, sfx__First-Last $A0-$CF, spec__First-Last
// $D0, flg__First-Last $E0-$E4). Game code should call these, not
// QueueSound1/2 directly.
void PlayMusic(uint8_t id);  // $81-$93, or $00/$80 to stop
void PlaySound(uint8_t id);  // $A0-$D0 (SFX/special SFX), or an $E0-$E4 command below

// The $E0-$E4 commands PlaySound() dispatches to directly (also callable on
// their own):
void StopAllSound(void);  // $E4 (and $00)
void FadeOutMusic(void);  // $E0
void PlaySegaSound(void); // $E1 -- NOT implemented yet (needs the "SEGA!" PCM
                           // sample decoded through a different codec path
                           // than the DPCM percussion -- see zPlay_SegaPCM)
void SpeedUpMusic(void);  // $E2
void SlowDownMusic(void); // $E3

// Matches PauseMusic/ResumeMusic: freezes/unfreezes sound_music's tempo
// governor (so it stops advancing playback) without touching sound_sfx --
// SFX keep playing while paused, same as the real driver.
void Sound_Pause(void);
void Sound_Resume(void);

// Advances playback by one 60Hz frame: drains the sound queues, ticks the
// tempo governor, and steps every active channel's byte-stream interpreter
// (writing PSG registers / decoding DPCM samples as it goes). Call this
// once per frame, before Sound_Generate().
void Sound_Frame(void);

// Diagnostic: when enabled, logs every note/duration/coordination-flag
// event TickChannel decodes to stderr, so a real song's playback can be
// verified directly against its own .asm source. Off by default.
void Sound_SetTrace(int enabled);

// Renders `count` mono samples at `sample_rate`, downmixing sound_music and
// sound_sfx additively into `out` (which is NOT cleared first by this
// function -- callers must zero it, since it's meant to be summed with
// whatever else shares the output buffer).
void Sound_Generate(int32_t *out, uint32_t count, uint32_t sample_rate);
