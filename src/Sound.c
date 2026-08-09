#include "Sound.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Diagnostic trace (see Sound_SetTrace): logs every note/duration event
// TickChannel decodes, so real song data can be verified against its own
// .asm source directly instead of guessing from audio alone.
static int sound_trace_enabled = 0;
void Sound_SetTrace(int enabled) { sound_trace_enabled = enabled; }
// Running Sound_Frame() count, printed as every trace line's prefix -- lets
// two channels' event timing be diffed by exact frame number instead of by
// eye, e.g. to check whether two channels sharing a voice for a rhythmically
// -locked part actually line up or have drifted apart.
static uint32_t sound_trace_frame = 0;
#define SOUND_TRACE(...)                          \
    do {                                            \
        if (sound_trace_enabled) {                  \
            fprintf(stderr, "[%u] ", sound_trace_frame); \
            fprintf(stderr, __VA_ARGS__);            \
        }                                            \
    } while (0)

// SFX channel IDs and DAC sample IDs -- matches smps2asmc/smps.h's cPSG1..
// cFM6/dKick.. constants (not #included directly here to avoid pulling in
// smps.h's SMPS_Byte()-emitting macro functions, which only make sense in
// the offline translator, not this runtime).
#define SND_cPSG1  0x80
#define SND_cPSG2  0xA0
#define SND_cPSG3  0xC0
#define SND_cNoise 0xE0
#define SND_cFM3   0x02
#define SND_cFM4   0x04
#define SND_cFM5   0x05
// No SND_cFM6 -- $06 is a Sonic 3/S&K/S3D-only channel ID ("overrides DAC");
// _smps2asm_inc.asm's smpsHeaderSFXChannel macro makes it a hard compile-time
// fatal for Sonic 1/2 driver versions ("unsupported... change it to another
// channel"), and no SFX file in this project uses it. Sonic 1's SFX RAM
// layout only ever allocates FM3-FM5 (3 channels) for regular SFX and a
// single fixed FM4 slot for the one special SFX ($D0, Waterfall).
#define SND_dKick  0x81

SoundChipSet sound_music;
SoundChipSet sound_sfx;

// YM2612 input clock: NTSC master clock / 7 (the Genesis feeds the FM chip
// a divided-down version of the same clock the PSG and 68000 share).
#define SOUND_FM_CLOCK (SOUND_PSG_CLOCK * 15 / 7) // 3579545 * 15/7 = 7670454 Hz

void Sound_Init(void) {
    SN76489_Init(&sound_music.psg);
    SN76489_Init(&sound_sfx.psg);
    sound_music.fm = YM2612_Create();
    sound_sfx.fm = YM2612_Create();
    // Default DAC pan (full L+R) -- LoadMusic() also sets this when a song
    // with its own DAC track loads, but PlaySegaSound_Trigger() (and any
    // other direct-to-DAC playback) runs independently of that, so without
    // this, sound_sfx.dac_pan would stay at its zero-initialized default
    // forever -- both L and R gates closed, silent regardless of whether
    // playback is actually triggered.
    sound_music.dac_pan = 0xC0;
    sound_sfx.dac_pan = 0xC0;
    // -1, not the zero-initialized default -- 0 is a real Hi-Timpani variant
    // index, and (for dac_pitch_override_sample) DAC_SAMPLE_KICK.
    sound_music.dac_timpani_variant = -1;
    sound_sfx.dac_timpani_variant = -1;
    sound_music.dac_pitch_override_sample = -1;
    sound_sfx.dac_pitch_override_sample = -1;
}

// ---------------------------------------------------------------------
// Sound ID dispatch table -- IDs match the hex byte baked into each real
// file's own name (Mus81 -> 0x81, SndD0 -> 0xD0, ...), so the table below
// is a direct, mechanical id->pointer mapping. Bit 7 of the ID (>=0x80) is
// always set on real IDs; a queued 0 means "nothing pending".
// ---------------------------------------------------------------------

#include "Resource/Music/Mus81_GHZ.h"
#include "Resource/Music/Mus82_LZ.h"
#include "Resource/Music/Mus83_MZ.h"
#include "Resource/Music/Mus84_SLZ.h"
#include "Resource/Music/Mus85_SYZ.h"
#include "Resource/Music/Mus86_SBZ.h"
#include "Resource/Music/Mus87_Invincibility.h"
#include "Resource/Music/Mus88_Extra_Life.h"
#include "Resource/Music/Mus89_Special_Stage.h"
#include "Resource/Music/Mus8A_Title_Screen.h"
#include "Resource/Music/Mus8B_Ending.h"
#include "Resource/Music/Mus8C_Boss.h"
#include "Resource/Music/Mus8D_FZ.h"
#include "Resource/Music/Mus8E_Sonic_Got_Through.h"
#include "Resource/Music/Mus8F_Game_Over.h"
#include "Resource/Music/Mus90_Continue_Screen.h"
#include "Resource/Music/Mus91_Credits.h"
#include "Resource/Music/Mus92_Drowning.h"
#include "Resource/Music/Mus93_Get_Emerald.h"
#ifdef SCP_SPLASH
#include "Resource/Music/Mus94_SSRG.h"
#endif

#include "Resource/Music/SndA0_Jump.h"
#include "Resource/Music/SndA1_Lamppost.h"
#include "Resource/Music/SndA2.h"
#include "Resource/Music/SndA3_Death.h"
#include "Resource/Music/SndA4_Skid.h"
#include "Resource/Music/SndA5.h"
#include "Resource/Music/SndA6_Hit_Spikes.h"
#include "Resource/Music/SndA7_Push_Block.h"
#include "Resource/Music/SndA8_SS_Goal.h"
#include "Resource/Music/SndA9_SS_Item.h"
#include "Resource/Music/SndAA_Splash.h"
#include "Resource/Music/SndAB.h"
#include "Resource/Music/SndAC_Hit_Boss.h"
#include "Resource/Music/SndAD_Get_Bubble.h"
#include "Resource/Music/SndAE_Fireball.h"
#include "Resource/Music/SndAF_Shield.h"
#include "Resource/Music/SndB0_Saw.h"
#include "Resource/Music/SndB1_Electric.h"
#include "Resource/Music/SndB2_Drown_Death.h"
#include "Resource/Music/SndB3_Flamethrower.h"
#include "Resource/Music/SndB4_Bumper.h"
#include "Resource/Music/SndB5_Ring.h"
#include "Resource/Music/SndB6_Spikes_Move.h"
#include "Resource/Music/SndB7_Rumbling.h"
#include "Resource/Music/SndB8.h"
#include "Resource/Music/SndB9_Collapse.h"
#include "Resource/Music/SndBA_SS_Glass.h"
#include "Resource/Music/SndBB_Door.h"
#include "Resource/Music/SndBC_Teleport.h"
#include "Resource/Music/SndBD_ChainStomp.h"
#include "Resource/Music/SndBE_Roll.h"
#include "Resource/Music/SndBF_Get_Continue.h"
#include "Resource/Music/SndC0_Basaran_Flap.h"
#include "Resource/Music/SndC1_Break_Item.h"
#include "Resource/Music/SndC2_Drown_Warning.h"
#include "Resource/Music/SndC3_Giant_Ring.h"
#include "Resource/Music/SndC4_Bomb.h"
#include "Resource/Music/SndC5_Cash_Register.h"
#include "Resource/Music/SndC6_Ring_Loss.h"
#include "Resource/Music/SndC7_Chain_Rising.h"
#include "Resource/Music/SndC8_Burning.h"
#include "Resource/Music/SndC9_Hidden_Bonus.h"
#include "Resource/Music/SndCA_Enter_SS.h"
#include "Resource/Music/SndCB_Wall_Smash.h"
#include "Resource/Music/SndCC_Spring.h"
#include "Resource/Music/SndCD_Switch.h"
#include "Resource/Music/SndCE_Ring_Left_Speaker.h"
#include "Resource/Music/SndCF_Signpost.h"
#include "Resource/Music/SndD0_Waterfall.h"

// Music IDs (0x81-0x93) use the smpsHeaderVoice/Chan/Tempo/DAC/FM/PSG
// layout (see LoadMusic); SFX IDs (0xA0-0xD0) use the shorter
// Voice/TempoSFX/ChanSFX/SFXChannel layout (see LoadSFX).
static const uint8_t *const sound_table[0x100] = {
    [0x81] = Mus81_GHZ,
    [0x82] = Mus82_LZ,
    [0x83] = Mus83_MZ,
    [0x84] = Mus84_SLZ,
    [0x85] = Mus85_SYZ,
    [0x86] = Mus86_SBZ,
    [0x87] = Mus87_Invincibility,
    [0x88] = Mus88_Extra_Life,
    [0x89] = Mus89_Special_Stage,
    [0x8A] = Mus8A_Title_Screen,
    [0x8B] = Mus8B_Ending,
    [0x8C] = Mus8C_Boss,
    [0x8D] = Mus8D_FZ,
    [0x8E] = Mus8E_Sonic_Got_Through,
    [0x8F] = Mus8F_Game_Over,
    [0x90] = Mus90_Continue_Screen,
    [0x91] = Mus91_Credits,
    [0x92] = Mus92_Drowning,
    [0x93] = Mus93_Get_Emerald,
#ifdef SCP_SPLASH
    [0x94] = Mus94_SSRG,
#endif

    [0xA0] = SndA0_Jump,
    [0xA1] = SndA1_Lamppost,
    [0xA2] = SndA2,
    [0xA3] = SndA3_Death,
    [0xA4] = SndA4_Skid,
    [0xA5] = SndA5,
    [0xA6] = SndA6_Hit_Spikes,
    [0xA7] = SndA7_Push_Block,
    [0xA8] = SndA8_SS_Goal,
    [0xA9] = SndA9_SS_Item,
    [0xAA] = SndAA_Splash,
    [0xAB] = SndAB,
    [0xAC] = SndAC_Hit_Boss,
    [0xAD] = SndAD_Get_Bubble,
    [0xAE] = SndAE_Fireball,
    [0xAF] = SndAF_Shield,
    [0xB0] = SndB0_Saw,
    [0xB1] = SndB1_Electric,
    [0xB2] = SndB2_Drown_Death,
    [0xB3] = SndB3_Flamethrower,
    [0xB4] = SndB4_Bumper,
    [0xB5] = SndB5_Ring,
    [0xB6] = SndB6_Spikes_Move,
    [0xB7] = SndB7_Rumbling,
    [0xB8] = SndB8,
    [0xB9] = SndB9_Collapse,
    [0xBA] = SndBA_SS_Glass,
    [0xBB] = SndBB_Door,
    [0xBC] = SndBC_Teleport,
    [0xBD] = SndBD_ChainStomp,
    [0xBE] = SndBE_Roll,
    [0xBF] = SndBF_Get_Continue,
    [0xC0] = SndC0_Basaran_Flap,
    [0xC1] = SndC1_Break_Item,
    [0xC2] = SndC2_Drown_Warning,
    [0xC3] = SndC3_Giant_Ring,
    [0xC4] = SndC4_Bomb,
    [0xC5] = SndC5_Cash_Register,
    [0xC6] = SndC6_Ring_Loss,
    [0xC7] = SndC7_Chain_Rising,
    [0xC8] = SndC8_Burning,
    [0xC9] = SndC9_Hidden_Bonus,
    [0xCA] = SndCA_Enter_SS,
    [0xCB] = SndCB_Wall_Smash,
    [0xCC] = SndCC_Spring,
    [0xCD] = SndCD_Switch,
    [0xCE] = SndCE_Ring_Left_Speaker,
    [0xCF] = SndCF_Signpost,
    [0xD0] = SndD0_Waterfall,
};

#define SOUND_ID_MUSIC_FIRST 0x81
#define SOUND_ID_MUSIC_LAST  0x93

// ---------------------------------------------------------------------
// DAC/DPCM percussion samples
// ---------------------------------------------------------------------

#include "Resource/DAC/bongo.h"
#include "Resource/DAC/clap.h"
#include "Resource/DAC/kick.h"
#include "Resource/DAC/scratch.h"
#include "Resource/DAC/snare.h"
#include "Resource/DAC/timpani.h"
#include "Resource/DAC/tom.h"
#include "Resource/PCM/sega.h"

typedef struct {
    const uint8_t *data;
    uint32_t length; // bytes (2 nibbles each)
    uint32_t rate;   // Hz -- real per-sample rate from the driver's zPCM_Table, NOT shared
} DACSample;

static const DACSample dac_samples[DAC_SAMPLE_COUNT] = {
    [DAC_SAMPLE_KICK] = {DAC_kick, sizeof(DAC_kick), 8250},
    [DAC_SAMPLE_SNARE] = {DAC_snare, sizeof(DAC_snare), 24000},
    // This is the DEFAULT/untriggered rate for plain Timpani ($83) only --
    // was 7250 Hz, confirmed by ear via ParadoxComposer's DAC preview to be
    // roughly an octave too low, doubled to 14500 Hz. Unlike Kick/Snare
    // above, no real disassembly source has confirmed this specific value
    // (the $88-$8B pitch-shifted variants below DO now have a real source
    // -- see timpani_variant_rate_hz -- but that table only covers the
    // post-override case; $83's own default before any $88-$8B has ever
    // fired is still this doubled-guess value, not independently verified).
    // Cross-referencing Sonic 2's own base*scale formula against this
    // project's real Timpani variant table implies a base closer to
    // ~7300-7500 Hz -- NOT changed, since Sonic 2's Kick/Snare/Timpani
    // sound the same as Sonic 1's but are confirmed to be different DPCM
    // recordings, so that formula doesn't transfer to this project's own
    // sample data.
    [DAC_SAMPLE_TIMPANI] = {DAC_timpani, sizeof(DAC_timpani), 14500},
    // Sonic 2-exclusive base samples (per your direction), folded into this
    // project's own extended DAC scheme -- see the DAC_SAMPLE_* enum
    // comment in Sound.h and the $81-$92 dispatch comment below. Base
    // rates all directly confirmed (not guessed/derived), same standard as
    // Kick/Snare above.
    [DAC_SAMPLE_SCRATCH] = {DAC_scratch, sizeof(DAC_scratch), 15000},
    [DAC_SAMPLE_CLAP] = {DAC_clap, sizeof(DAC_clap), 17000},
    [DAC_SAMPLE_TOM] = {DAC_tom, sizeof(DAC_tom), 13500},
    [DAC_SAMPLE_BONGO] = {DAC_bongo, sizeof(DAC_bongo), 7375},
};

// Real driver's DAC_sample_rate table (byte_71CC4, referenced from
// DACUpdateTrack) -- each entry is dpcmLoopCounter(Hz) for note bytes
// $88-$8B (Hi/Mid/Low/Floor), transcribed directly from the real
// disassembly rather than an earlier scale-factor approximation this
// replaced (1.30/1.20/0.97/0.95 off the -- itself uncertain -- base rate
// above). Real Sonic 1's own table continues with $8C/$8D (both
// dpcmLoopCounter saturating at $FF, i.e. "so slow you may want to skip
// them" per its own comment) and $8E/$8F (no entry at all) -- not
// implemented here, since this project's own $8C+ range is used for Tom
// variants instead (see the $81-$92 dispatch comment below).
static const uint32_t timpani_variant_rate_hz[4] = {9750, 8750, 7150, 7000};

// Tom/Bongo pitch-shifted variants (note bytes $8C-$8E/$8F-$91) -- Sonic
// 2-exclusive, per your direction. Unlike Timpani above, no standalone
// absolute-Hz disassembly table was available for these; instead
// transcribed from Sonic 2's own dac_sample_metadata macro (base_rate *
// scale, scale factors 1.70/1.30/1.10 for Tom and 2.00/1.75/1.30 for
// Bongo) applied against DAC_SAMPLE_TOM/BONGO's own confirmed base rates
// above (13500 Hz, 7375 Hz), then rounded to the nearest whole Hz.
static const uint32_t tom_variant_rate_hz[3] = {22950, 17550, 14850};   // $8C/$8D/$8E: Mid/Low/Floor-Tom
static const uint32_t bongo_variant_rate_hz[3] = {14750, 12906, 9588}; // $8F/$90/$91: Hi/Mid/Low-Bongo

// JMan2050's DAC decode table (see zDACDecodeTbl in the real driver): each
// 4-bit nibble is a signed step added to an 8-bit accumulator, giving cheap
// exponential (not linear) DPCM compression -- small nibbles for fine
// detail near silence, large nibbles for fast transients like a kick's
// attack. Purely a mathematical progression (powers of two, then their
// negatives), not driver-specific data.
static const int8_t dac_delta_table[16] = {
    0, 1, 2, 4, 8, 0x10, 0x20, 0x40, (int8_t)0x80,
    -1, -2, -4, -8, -0x10, -0x20, -0x40,
};

// The "SEGA!" boot voice clip (res/PCM/sega) is plain unsigned 8-bit mono
// PCM -- no header, no delta-decode, just raw samples written straight to
// the DAC output register one at a time (see zPlay_SegaPCM/
// zPlaySEGAPCMLoop). 16000 is the Sonic 1 driver's own named pitch-loop
// rate constant (pcmLoopCounter(16000)) -- the Sonic 3 sample is format-
// compatible but runs faster (its own WAV header gives 33598 Hz), so this
// value has to match whichever sample actually ends up in res/PCM/sega.
#define SEGA_PCM_HZ 16000

static void PlaySegaSound_Trigger(SoundChipSet *cs) {
    cs->pcm_pos = 0;
    cs->pcm_playing = 1;
    cs->pcm_phase = 0;
}

static void PCM_Generate(SoundChipSet *cs, int32_t *out, uint32_t count, uint32_t sample_rate) {
    if (!cs->pcm_playing)
        return;
    int left = (cs->dac_pan & 0x80) != 0;
    int right = (cs->dac_pan & 0x40) != 0;
    for (uint32_t i = 0; i < count; i++) {
        if (!cs->pcm_playing)
            break;
        int32_t s = ((int32_t)PCM_sega[cs->pcm_pos] - 0x80) << 6; // roughly matches PSG/DPCM amplitude scale
        if (left)
            out[2 * i + 0] += s;
        if (right)
            out[2 * i + 1] += s;

        cs->pcm_phase += SEGA_PCM_HZ;
        while (cs->pcm_phase >= sample_rate) {
            cs->pcm_phase -= sample_rate;
            cs->pcm_pos++;
            if (cs->pcm_pos >= sizeof(PCM_sega)) {
                cs->pcm_playing = 0;
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------
// PSG volume envelopes (res/PSG/psg1-9.bin -- matches fTone_01..fTone_09
// in smps.h, the smpsHeaderPSG "voice" argument). Each is a sequence of
// signed per-tick deltas added cumulatively to a channel's base volume
// (PSGUpdateVolFX in the real driver); $80 is the terminator -- once hit,
// the envelope holds at its current value forever (VolEnvHold just
// decrements the index back so the same terminator keeps getting re-read).
// ---------------------------------------------------------------------

#include "Resource/PSG/psg1.h"
#include "Resource/PSG/psg2.h"
#include "Resource/PSG/psg3.h"
#include "Resource/PSG/psg4.h"
#include "Resource/PSG/psg5.h"
#include "Resource/PSG/psg6.h"
#include "Resource/PSG/psg7.h"
#include "Resource/PSG/psg8.h"
#include "Resource/PSG/psg9.h"

typedef struct {
    const uint8_t *data;
    uint32_t length;
} PSGEnvelope;

#define PSG_ENVELOPE_COUNT 9
static const PSGEnvelope psg_envelopes[PSG_ENVELOPE_COUNT] = {
    {PSG_psg1, sizeof(PSG_psg1)}, {PSG_psg2, sizeof(PSG_psg2)}, {PSG_psg3, sizeof(PSG_psg3)},
    {PSG_psg4, sizeof(PSG_psg4)}, {PSG_psg5, sizeof(PSG_psg5)}, {PSG_psg6, sizeof(PSG_psg6)},
    {PSG_psg7, sizeof(PSG_psg7)}, {PSG_psg8, sizeof(PSG_psg8)}, {PSG_psg9, sizeof(PSG_psg9)},
};

// Steps one channel's volume envelope by one tick and returns the resulting
// PSG attenuation (0-15), or -1 if nothing changed (envelope disabled, at
// rest, or held at its terminator -- caller should leave the register
// alone). ch->voice_index selects the envelope (0 = none, 1-9 = psg_envelopes
// index); ch->vol_env_index is the current position within it.
static int PSGStepEnvelope(SoundChannel *ch) {
    if (ch->voice_index == 0 || ch->voice_index > PSG_ENVELOPE_COUNT)
        return -1;
    const PSGEnvelope *env = &psg_envelopes[ch->voice_index - 1];
    if (ch->vol_env_index >= env->length)
        return -1; // Out of range data -- treat as silently held, don't walk off the array

    int8_t delta = (int8_t)env->data[ch->vol_env_index];
    if ((uint8_t)delta == 0x80)
        return -1; // Terminator -- hold (index does NOT advance)
    ch->vol_env_index++;

    int vol = (int)ch->volume + delta;
    if (vol < 0 || vol > 0x0F)
        vol = 0x0F; // Real driver clamps both overflow AND underflow to silence, not to loudest
    return vol;
}

static void DAC_Trigger(SoundChipSet *cs, int sample_id) {
    if (sample_id < 0 || sample_id >= DAC_SAMPLE_COUNT)
        return;
    cs->dac_data = dac_samples[sample_id].data;
    cs->dac_sample_id = sample_id;
    cs->dac_nibble_count = dac_samples[sample_id].length * 2;
    cs->dac_rate = (sample_id == cs->dac_pitch_override_sample) ? cs->dac_pitch_override_rate
                                                                  : dac_samples[sample_id].rate;
    cs->dac_nibble_pos = 0;
    cs->dac_accum = 0x80;
    cs->dac_playing = 1;
    cs->dac_phase = 0;
}

// Steps DAC playback by `count` output samples at `sample_rate`, adding the
// decoded (and centered) waveform into `out`, at the currently-triggered
// sample's own real rate (see dac_samples[]/DAC_Trigger) -- not a shared
// fixed rate; kick/snare/timpani genuinely differ in the real driver's own
// zPCM_Table. Ignores the DAC header's pitch byte -- per-sample pitch
// shifting (used for the timpani variants) isn't implemented yet.
static void DAC_Generate(SoundChipSet *cs, int32_t *out, uint32_t count, uint32_t sample_rate) {
    if (!cs->dac_playing)
        return;
    int left = (cs->dac_pan & 0x80) != 0;
    int right = (cs->dac_pan & 0x40) != 0;
    for (uint32_t i = 0; i < count; i++) {
        if (!cs->dac_playing)
            break;
        int32_t s = ((int32_t)cs->dac_accum - 0x80) << 6; // roughly matches PSG's amplitude scale
        if (left)
            out[2 * i + 0] += s;
        if (right)
            out[2 * i + 1] += s;

        cs->dac_phase += cs->dac_rate;
        while (cs->dac_phase >= sample_rate) {
            cs->dac_phase -= sample_rate;
            if (cs->dac_nibble_pos >= cs->dac_nibble_count) {
                cs->dac_playing = 0;
                break;
            }
            uint8_t byte = cs->dac_data[cs->dac_nibble_pos / 2];
            uint8_t nibble = (cs->dac_nibble_pos & 1) ? (byte >> 4) : (byte & 0x0F);
            int step = dac_delta_table[nibble];
            int accum = (int)cs->dac_accum + step;
            if (accum < 0)
                accum = 0;
            if (accum > 0xFF)
                accum = 0xFF;
            cs->dac_accum = (uint8_t)accum;
            cs->dac_nibble_pos++;
        }
    }
}

// ---------------------------------------------------------------------
// PSG register helpers (SN76489 latch/data write protocol)
// ---------------------------------------------------------------------

static void PSG_SetAttenuation(SN76489 *chip, int channel, uint8_t atten4) {
    if (atten4 > 0x0F)
        atten4 = 0x0F;
    SN76489_Write(chip, (uint8_t)(0x80 | (channel << 5) | 0x10 | atten4));
}

static void PSG_SetTonePeriod(SN76489 *chip, int channel, uint16_t period10) {
    if (period10 > 0x3FF)
        period10 = 0x3FF;
    SN76489_Write(chip, (uint8_t)(0x80 | (channel << 5) | (period10 & 0x0F)));
    SN76489_Write(chip, (uint8_t)((period10 >> 4) & 0x3F));
}

// ---------------------------------------------------------------------
// YM2612 register helpers
// ---------------------------------------------------------------------

// channel_index here is SOUND_CHANNEL_FM_BASE..+5 (real FM channels 1-6,
// SOUND_CHANNEL_DAC excluded -- the DAC doesn't have YM2612 registers).
// Returns the write offset (0/1 = port 1 addr/data for channels 1-3,
// 2/3 = port 2 for channels 4-6) and the in-port channel number (0-2).
static void FMPortChannel(int channel_index, int *port_offset, int *chan_in_port) {
    int fm_index = channel_index - SOUND_CHANNEL_FM_BASE; // 0-5
    *port_offset = (fm_index < 3) ? 0 : 2;
    *chan_in_port = fm_index % 3;
}

static void FM_WriteReg(YM2612 *fm, int port_offset, uint8_t reg, uint8_t data) {
    if (getenv("SONIC_FM_TRACE"))
        fprintf(stderr, "FMREG port=%d reg=$%02X data=$%02X\n", port_offset, reg, data);
    YM2612_Write(fm, (uint32_t)port_offset, reg);
    YM2612_Write(fm, (uint32_t)port_offset + 1, data);
}

// TL (Total Level) is attenuation, same convention as PSG -- 0 = loudest,
// higher = quieter. Real max is 127 (7-bit).
static uint8_t FM_ClampTL(int tl) { return (uint8_t)(tl < 0 ? 0 : (tl > 127 ? 127 : tl)); }

// Applies sch->volume as a live offset to every operator's cached base TL
// and writes it to hardware immediately -- used by $E6 (designed for FM,
// "effective immediately" per the coordination-flag table). $EC (designed
// for PSG) instead just updates sch->volume without calling this -- for FM
// channels that only takes effect "on next voice change", i.e. the next
// time FM_LoadVoice folds sch->volume in below.

// Real driver's FMSlotMask (s1.sounddriver.asm): per-algorithm bitmask of
// which operators are "carriers" -- the ones an algorithm actually routes
// to audible output. Only carriers get the track's volume attenuation
// added to their TL; modulator operators keep their authored TL untouched,
// since changing a modulator's level changes the FM modulation depth (the
// timbre itself), not just the loudness. Indexed by algorithm (0-7 real
// hardware, 8-15 fmcore-original -- see fm_voice.c's own comment on each);
// bit order matches the real table's own op1,op3,op2,op4 write order (SCHG
// FM voice format table / FMInstrumentTLTable -- see FM_LoadVoice's own
// comment), not natural op order -- FM_SLOT_BIT[] below remaps logical op
// index to that bit position. (This was previously {3,2,1,0}, derived
// against the op4,op3,op2,op1 order used before that fix -- left stale when
// the write order changed, silently marking op1 as algorithm 0-3's carrier
// instead of the real op4, and similarly wrong elsewhere.) Entries 8-15
// derived directly from FM_VOICE_ALGORITHM_CONNECT[8..15]'s own carrier
// sets (fmcore/fm_voice.c), same FM_SLOT_BIT remap applied for consistency.
static const uint8_t FM_SLOT_MASK[16] = {8, 8, 8, 8, 0xA, 0xE, 0xE, 0xF, 8, 0xC, 8, 8, 0xA, 0xF, 0xF, 0xF};
static const int FM_SLOT_BIT[4] = {0, 2, 1, 3}; // natural op1..op4 -> FMSlotMask bit position

static int FM_IsCarrier(uint8_t algorithm, int op) {
    return (FM_SLOT_MASK[algorithm & 0xF] >> FM_SLOT_BIT[op]) & 1;
}

static void FM_ApplyVolume(YM2612 *fm, SoundChannel *sch, int channel_index) {
    int port, ch;
    FMPortChannel(channel_index, &port, &ch);
    for (int op = 0; op < 4; op++) {
        int slot = op * 4; // register offset per operator is natural: op1=+0x00, op2=+0x04, op3=+0x08, op4=+0x0C
        int is_carrier = FM_IsCarrier(sch->feedback_algo, op);
        int add = is_carrier ? sch->volume : 0;
        // Debug-only mute (ParadoxComposer's Channels panel): only carrier
        // operators reach the audible mix, so only those need forcing to
        // max attenuation -- modulator operators keep running normally
        // (they're inaudible on their own either way), which also means
        // un-muting doesn't need to restore any modulator state.
        uint8_t tl = (sch->debug_muted && is_carrier) ? 0x7F : FM_ClampTL(sch->fm_base_tl[op] + add);
        FM_WriteReg(fm, port, (uint8_t)(0x40 + slot + ch), tl);
    }
}

// Loads one 25-byte voice block (see smps.c's smpsVcTotalLevel comment for
// the exact layout this mirrors) into a channel's 4 operators + algorithm/
// feedback registers.
static void FM_LoadVoice(YM2612 *fm, SoundChannel *sch, int channel_index, const uint8_t *voice) {
    int port, ch;
    FMPortChannel(channel_index, &port, &ch);

    // Algorithm is 4 bits (0-7 real hardware, 8-15 fmcore-original -- see
    // fm_voice.c's own comment): low 3 bits as always, plus bit7 as the
    // high algorithm bit ("0"=real algorithm 0-7, "1"=custom fmcore
    // algorithm 8-15). Real hardware register $B0 leaves bits 6-7 unused/
    // ignored, so this is harmless if this exact byte were ever loaded on
    // real silicon (or the ymfm backend) -- it just sees algorithm 0-7's
    // low bits and silently ignores bit7. Feedback stays bits3-5, unchanged
    // from real hardware's own layout.
    sch->feedback_algo = (uint8_t)((((voice[0] >> 7) & 1) << 3) | (voice[0] & 7));
    FM_WriteReg(fm, port, (uint8_t)(0xB0 + ch), voice[0]); // algorithm(+high bit)/feedback -- every bit meaningful or genuinely inert on both backends

    // voice[] is stored op1,op3,op2,op4 per row (see smps.c's
    // smpsVcTotalLevel comment -- matches the SCHG FM voice format table
    // and the real driver's FMInstrumentOperatorTable register-write
    // sequence). For natural operator index op (0=op1..3=op4), its byte
    // position within each row is pos_map[op] below (self-inverse: op2 and
    // op3 swap positions, op1/op4 stay put). The YM2612 register address
    // per operator is separately just op*4, naturally sequential -- no
    // hardware remap needed.
    static const int pos_map[4] = {0, 2, 1, 3};
    for (int op = 0; op < 4; op++) {
        int slot = op * 4;
        int pos = pos_map[op];
        FM_WriteReg(fm, port, (uint8_t)(0x30 + slot + ch), voice[1 + pos]);  // DT/MUL
        FM_WriteReg(fm, port, (uint8_t)(0x50 + slot + ch), voice[5 + pos]);  // RS/AR
        FM_WriteReg(fm, port, (uint8_t)(0x60 + slot + ch), voice[9 + pos]);  // AM/D1R
        FM_WriteReg(fm, port, (uint8_t)(0x70 + slot + ch), voice[13 + pos]); // D2R
        FM_WriteReg(fm, port, (uint8_t)(0x80 + slot + ch), voice[17 + pos]); // D1L/RR

        uint8_t base_tl = voice[21 + pos] & 0x7F;
        sch->fm_base_tl[op] = base_tl;
        int add = FM_IsCarrier(sch->feedback_algo, op) ? sch->volume : 0;
        FM_WriteReg(fm, port, (uint8_t)(0x40 + slot + ch), FM_ClampTL(base_tl + add));
    }
}

// Pan direction is whole-channel only -- hard left, hard right, center
// (both), or silent (neither) -- not a continuous position. panLeft/
// panRight/panCentre/panNone in smps.h already match the L/R bits of
// register $B4 directly, and the SAME byte's bits5-4 (AMS) are a real,
// supported register field too, so the coordination-flag parameter byte is
// written through unmasked instead of zeroing them. AMS only has an
// audible effect once something enables the chip-wide LFO (register $22)
// -- no coordination flag writes that yet, so today AMS bits are silently
// inert, exactly as they'd be on real hardware. bits2-0 (FMS/PMS, pitch
// modulation depth) are a deliberate fixed 0/off, not a gap -- see
// YM2612_FMCore.c's own $B4-$B6 write handler comment for why.
static void FM_SetPan(YM2612 *fm, int channel_index, uint8_t value) {
    int port, ch;
    FMPortChannel(channel_index, &port, &ch);
    FM_WriteReg(fm, port, (uint8_t)(0xB4 + ch), value);
}

static void FM_KeyOnOff(YM2612 *fm, int channel_index, int on) {
    // Diagnostic isolation only (SonicSoundWav): SONIC_FM_CHANNEL=N (0-5)
    // silences every other FM channel's key-on so the real ROM voice/note
    // data for just that one channel can be inspected/heard alone.
    const char *only = getenv("SONIC_FM_CHANNEL");
    if (only && (channel_index - SOUND_CHANNEL_FM_BASE) != atoi(only))
        on = 0;
    int port, ch;
    FMPortChannel(channel_index, &port, &ch);
    int chan_code = ch + ((channel_index - SOUND_CHANNEL_FM_BASE >= 3) ? 4 : 0);
    uint8_t op_mask = on ? 0xF0 : 0x00; // all 4 operators on/off together
    if (getenv("SONIC_FM_TRACE"))
        fprintf(stderr, "FMKEY channel_index=%d chan_code=$%02X on=%d\n", channel_index, chan_code, on);
    YM2612_Write(fm, 0, 0x28);
    YM2612_Write(fm, 1, (uint8_t)(op_mask | chan_code));
}

// Real hardware F-Number table (12 semitones, block-0), cross-referenced
// against ValleyBell's SMPSPlay (github.com/sonicretro/SKC-SMPSOUT's
// SMPSPlay submodule, Engine/smps.c GetNote + SMPSPlay-DLL.cpp's
// DEF_FMFREQ_VAL) -- the real driver's own quantized per-semitone table,
// not a from-scratch pow()/log2() formula, which was producing audibly
// slightly-off tuning versus a real Genesis. The table is indexed starting
// from B, not C (13th entry is the next octave's B, unused here since we
// wrap via %12 ourselves) -- SMPSPlay's own GetNote compensates for this by
// adding 1 to the note number for FM specifically (FMBASEN_B) before
// indexing; we do the same below rather than reordering the table, so it
// stays a direct, checkable transcription of the source values.
static const uint16_t FM_FNUM_TABLE[12] = {0x25E, 0x284, 0x2AB, 0x2D3, 0x2FE, 0x32D,
                                            0x35C, 0x38F, 0x3C5, 0x3FF, 0x43C, 0x47C};

static void FM_SetFrequency(YM2612 *fm, int channel_index, int note_index, int mod_val) {
    if (note_index < 0)
        note_index = 0;
    int shifted = note_index + 1; // FMBASEN_B: realigns note_index==0 (nC0) onto the B-based table
    int octave = shifted / 12;
    int fnum = (int)FM_FNUM_TABLE[shifted % 12] + mod_val;
    if (octave > 7)
        octave = 7; // Defensive only -- real driver (FMOctWrap=0) trusts song data to never exceed this
    if (fnum < 0)
        fnum = 0;
    if (fnum > 2047)
        fnum = 2047;

    int port, ch;
    FMPortChannel(channel_index, &port, &ch);
    FM_WriteReg(fm, port, (uint8_t)(0xA4 + ch), (uint8_t)((octave << 3) | (fnum >> 8)));
    FM_WriteReg(fm, port, (uint8_t)(0xA0 + ch), (uint8_t)(fnum & 0xFF));
}

// Real hardware PSG period table (6 octaves), same source as FM_FNUM_TABLE
// above (DEF_PSGFREQ_68K_VAL) -- PSGBaseNote defaults to PSGBASEN_C (no
// index shift needed, unlike FM's table-starts-at-B quirk). The trailing 0
// is the real table's own terminator entry, kept here so an out-of-range
// clamp (matching GetNote's "Note >= PSGFreqCnt -> PSGFreqCnt-1") lands on
// the same value real hardware would.
static const uint16_t PSG_PERIOD_TABLE[70] = {
    0x356, 0x326, 0x2F9, 0x2CE, 0x2A5, 0x280, 0x25C, 0x23A, 0x21A, 0x1FB, 0x1DF, 0x1C4, 0x1AB, 0x193, 0x17D,
    0x167, 0x153, 0x140, 0x12E, 0x11D, 0x10D, 0x0FE, 0x0EF, 0x0E2, 0x0D6, 0x0C9, 0x0BE, 0x0B4, 0x0A9, 0x0A0,
    0x097, 0x08F, 0x087, 0x07F, 0x078, 0x071, 0x06B, 0x065, 0x05F, 0x05A, 0x055, 0x050, 0x04B, 0x047, 0x043,
    0x040, 0x03C, 0x039, 0x036, 0x033, 0x030, 0x02D, 0x02B, 0x028, 0x026, 0x024, 0x022, 0x020, 0x01F, 0x01D,
    0x01B, 0x01A, 0x018, 0x017, 0x016, 0x015, 0x013, 0x012, 0x011, 0x000};

// note_index: 0 == C0, 1 semitone per step (already includes transpose).
static uint16_t PSGPeriodForNote(int note_index) {
    if (note_index < 0)
        note_index = 0;
    if (note_index >= (int)(sizeof(PSG_PERIOD_TABLE) / sizeof(PSG_PERIOD_TABLE[0])))
        note_index = (int)(sizeof(PSG_PERIOD_TABLE) / sizeof(PSG_PERIOD_TABLE[0])) - 1;
    uint16_t period = PSG_PERIOD_TABLE[note_index];
    if (period < 1)
        period = 1;
    if (period > 1023)
        period = 1023;
    return period;
}

// ---------------------------------------------------------------------
// Song/SFX header loading
// ---------------------------------------------------------------------

static uint16_t ReadWord(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

static void ResetChannel(SoundChannel *ch) { memset(ch, 0, sizeof(*ch)); }

static void StartChannel(SoundChipSet *cs, SoundChannel *ch, const uint8_t *data, int8_t transpose, uint8_t volume) {
    ResetChannel(ch);
    ch->data_ptr = data;
    ch->transpose = transpose;
    ch->volume = volume;
    ch->tempo_divider = cs->duration_mult; // Per-track duration multiplier, overridable by $E5
    ch->active = 1;
    ch->duration_timeout = 0; // read the first command immediately on the next tick
}

static void LoadMusic(SoundChipSet *cs, const uint8_t *song, uint8_t music_id) {
    const uint8_t *p = song;
    uint16_t voice_off = ReadWord(p);
    p += 2;
    cs->voice_bank = song + voice_off;
    uint8_t fm_count = *p++;
    uint8_t psg_count = *p++;
    cs->psg_count = psg_count; // Cached for $F3's "only if psg_count<4" gate
    cs->duration_mult = *p++;
    cs->main_tempo = *p++;
    cs->base_main_tempo = cs->main_tempo; // Cached so SlowDownMusic can restore it
    cs->current_music_id = music_id;      // For SpeedUpMusic's per-song SpeedUpIndex lookup
    cs->tempo_timeout = cs->main_tempo;

    // The byte stream's DAC/FM region always has exactly fm_count total
    // 4-byte blocks: the FIRST is always the DAC block (word ptr + pitch +
    // vol), matching the real driver's FMDACInitBytes table (DAC's control
    // byte is literally the first entry, loaded on the loop's first pass);
    // the remaining fm_count-1 blocks are FM1..FM(fm_count-1) in order.
    // fm_count==7 is not a different layout -- it's the same DAC-first
    // rule, just with all 6 FM blocks also present. Real songs that don't
    // want DAC output in that mode encode the DAC block's own data as a
    // bare smpsStop by convention (see Mus89_Special_Stage); the real
    // driver additionally clears the YM2612's $2B DAC-enable register as a
    // hardware-level safety net, which our independent software DAC mixer
    // doesn't need to model since it never shares state with FM6.
    // fm_count==0 means there's no DAC-position block in the stream at
    // all -- not even one. The whole FM/DAC region is only present when
    // fm_count>0 (this is what "0000" -- an empty voice-bank-only song --
    // means: skip straight to PSG parsing, no read here at all).
    if (fm_count > 0) {
        uint16_t dac_off = ReadWord(p);
        p += 2;
        int8_t dac_pitch = (int8_t)*p++;
        uint8_t dac_vol = *p++;
        StartChannel(cs, &cs->channels[SOUND_CHANNEL_DAC], song + dac_off, dac_pitch, dac_vol);
        cs->dac_pan = 0xC0; // Default: full L+R, until/unless the DAC track's own $E0 changes it
    } else {
        ResetChannel(&cs->channels[SOUND_CHANNEL_DAC]);
    }

    // FM6 only gets real track data when fm_count==7 (matching the real
    // driver's loop, which only reaches FM6's slot on a 7th pass); for
    // any fm_count<7 it's explicitly force-silenced here, same as the real
    // driver's .silencefm6 register writes for every non-7 fm_count value
    // (not just fm_count==6).
    int real_fm_blocks = (fm_count > 0) ? fm_count - 1 : 0;
    for (int i = 0; i < 6; i++) {
        SoundChannel *ch = &cs->channels[SOUND_CHANNEL_FM_BASE + i];
        if (i < real_fm_blocks) {
            uint16_t off = ReadWord(p);
            p += 2;
            int8_t pitch = (int8_t)*p++;
            uint8_t vol = *p++;
            StartChannel(cs, ch, song + off, pitch, vol);
        } else {
            ResetChannel(ch);
        }
    }

    for (int i = 0; i < SOUND_CHANNELS_PSG; i++) {
        SoundChannel *ch = &cs->channels[SOUND_CHANNEL_PSG_BASE + i];
        if (i < psg_count) {
            uint16_t off = ReadWord(p);
            p += 2;
            int8_t pitch = (int8_t)*p++;
            uint8_t vol = *p++;
            uint8_t mod = *p++;
            uint8_t voice = *p++;
            StartChannel(cs, ch, song + off, pitch, vol);
            (void)mod; // Real driver reads this byte into a scratch register and never stores it anywhere -- the
                       // envelope index always starts wherever it was left (immediately overwritten by the first
                       // real note/rest/bare-duration event's own reset anyway, see FinishTrackUpdate).
            ch->voice_index = voice;
            // The 4th PSG slot (psg_count==4) is a dedicated hardware noise
            // channel, not a redirectable tone channel -- its own note data
            // always drives SN76489 channel 3's noise control, same as a
            // $F3-redirected PSG3 track, just without needing the $F3 flag
            // (which is why $F3 is a no-op whenever psg_count==4: this slot
            // already covers it).
            if (i == 3)
                ch->psg_noise = 1;
            SOUND_TRACE("psg%d: off=$%04X first bytes: %02X %02X %02X %02X %02X %02X %02X %02X\n", i, off,
                        ch->data_ptr[0], ch->data_ptr[1], ch->data_ptr[2], ch->data_ptr[3], ch->data_ptr[4],
                        ch->data_ptr[5], ch->data_ptr[6], ch->data_ptr[7]);
        } else {
            ResetChannel(ch);
        }
    }

    cs->dac_playing = 0; // any DPCM sample mid-playback belongs to the old song
}

static int SFXChannelIndex(uint8_t chanid) {
    switch (chanid) {
        case SND_cPSG1: return SOUND_CHANNEL_PSG_BASE + 0;
        case SND_cPSG2: return SOUND_CHANNEL_PSG_BASE + 1;
        case SND_cPSG3: return SOUND_CHANNEL_PSG_BASE + 2;
        case SND_cNoise: return SOUND_CHANNEL_PSG_BASE + 3; // the extra 4th PSG slot -- see Sound.h
        case SND_cFM3: return SOUND_CHANNEL_FM_BASE + 2;
        case SND_cFM4: return SOUND_CHANNEL_FM_BASE + 3;
        case SND_cFM5: return SOUND_CHANNEL_FM_BASE + 4;
        default: return -1;
    }
}

// SFX only ever occupy a handful of channels (never all of them) -- unlike
// music, a new SFX must NOT reset channels it doesn't itself target, since
// those may belong to whatever's already playing (e.g. music sharing the
// chip set, or another SFX's other channels).
static void LoadSFX(SoundChipSet *cs, const uint8_t *song) {
    const uint8_t *p = song;
    p += 2; // voice bank pointer
    // SFX headers only carry a dividing-timing (duration multiplier) byte
    // -- no main_tempo/periodic-correction byte at all, so SFX playback
    // just runs at an uncorrected 1:1 frame rate scaled by that multiplier
    // (main_tempo=0 disables the periodic +1 correction in TickChipSet).
    cs->duration_mult = *p++;
    cs->main_tempo = 0;
    cs->tempo_timeout = 0;
    uint8_t chan_count = *p++;

    for (int i = 0; i < chan_count; i++) {
        p++; // marker byte, always 0x80
        uint8_t chanid = *p++;
        uint16_t off = ReadWord(p);
        p += 2;
        int8_t pitch = (int8_t)*p++;
        uint8_t vol = *p++;

        int idx = SFXChannelIndex(chanid);
        if (idx < 0)
            continue;
        StartChannel(cs, &cs->channels[idx], song + off, pitch, vol);
    }
}

void QueueSound1(uint8_t id) { sound_music.queue[SOUND_QUEUE_NORMAL] = id; }
void QueueSound2(uint8_t id) { sound_sfx.queue[SOUND_QUEUE_SPECIAL] = id; }

static void DispatchQueue(SoundChipSet *cs, int slot) {
    uint8_t id = cs->queue[slot];
    if (id == 0)
        return;
    cs->queue[slot] = 0;

    const uint8_t *song = sound_table[id];
    if (!song)
        return; // unmapped ID (a Snd*/Mus* file this project doesn't have, or a driver-2-only ID)

    if (id >= SOUND_ID_MUSIC_FIRST && id <= SOUND_ID_MUSIC_LAST)
        LoadMusic(cs, song, id);
    else
        LoadSFX(cs, song);
}

// ---------------------------------------------------------------------
// High-level API: PlayMusic/PlaySound, special $E0-$E4 commands, pause
// ---------------------------------------------------------------------

#define SOUND_ID_SFX_FIRST     0xA0
#define SOUND_ID_SFX_LAST      0xCF
#define SOUND_ID_SPECIAL_FIRST 0xD0
#define SOUND_ID_SPECIAL_LAST  0xD0
#define SOUND_ID_FLAG_FIRST    0xE0
#define SOUND_ID_FLAG_LAST     0xE4

// Matches SoundPriorities: higher wins. Indexed by id - SOUND_ID_SFX_FIRST,
// covering $A0-$D0 -- purely a numeric priority-arbitration table, one byte
// per sound ID, mechanically transcribed.
static const uint8_t sound_priorities[SOUND_ID_SPECIAL_LAST - SOUND_ID_SFX_FIRST + 1] = {
    0x80, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x68, 0x70, 0x70, 0x70, 0x60, 0x70, // $A0
    0x70, 0x60, 0x70, 0x60, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x7F, // $B0
    0x60, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, // $C0
    0x80,                                                                                           // $D0
};

void PlayMusic(uint8_t id) {
    if (id == 0 || id == 0x80) {
        // real "silence" sentinel -- treat like a stop rather than queuing
        StopAllSound();
        return;
    }
    QueueSound1(id);
}

void StopAllSound(void) {
    memset(&sound_music.channels, 0, sizeof(sound_music.channels));
    memset(&sound_sfx.channels, 0, sizeof(sound_sfx.channels));
    sound_music.dac_playing = 0;
    sound_sfx.dac_playing = 0;
    // Deliberately NOT touching pcm_playing here: on real hardware, the
    // Z80 busy-waits on the "SEGA!" clip, so any command sent from the 68K
    // side (including a title-screen StopAllSound) effectively queues up
    // behind it and only takes effect once the Z80 gets back to checking
    // for new commands -- the clip always finishes playing regardless of
    // what the 68K does next. This port has no such serial blocking, so
    // letting StopAllSound() leave an in-progress PCM clip alone is what
    // reproduces that behavior instead.
    sound_sfx.current_priority = 0;
    for (int i = 0; i < 3; i++)
        PSG_SetAttenuation(&sound_music.psg, i, 0x0F);
    for (int i = 0; i < 4; i++)
        PSG_SetAttenuation(&sound_sfx.psg, i, 0x0F);
}

void FadeOutMusic(void) {
    // TODO: real behavior ramps volume down over several frames via a
    // dedicated fade coordination step; this just cuts it immediately.
    memset(&sound_music.channels, 0, sizeof(sound_music.channels));
    sound_music.dac_playing = 0;
    for (int i = 0; i < 3; i++)
        PSG_SetAttenuation(&sound_music.psg, i, 0x0F);
}

void PlaySegaSound(void) {
    // Real hardware plays this on sound_sfx's physical DAC, exclusively --
    // it busy-waits the Z80 for the clip's duration so nothing else can use
    // the DAC meanwhile. We don't need the busy-wait (this just triggers
    // playback and returns; PCM_Generate steps it per-frame), but route it
    // through sound_sfx to match which chip set owns it.
    PlaySegaSound_Trigger(&sound_sfx);
}

// Real driver swaps in a separate stored "speedup tempo" constant here,
// not derived from the song's own main_tempo -- approximated by nudging
// main_tempo instead, since we don't track that separate constant.
// Higher main_tempo = fewer periodic +1 corrections = faster playback.
// SpeedUpIndex: real per-song replacement main_tempo values used while
// speed shoes are active, indexed by (music_id - bgm_GHZ). Only the first
// 8 songs (GHZ..ExtraLife) have real entries in the original driver -- for
// any song past that, the real table read continues past SpeedUpIndex's
// end into MusicIndex's own pointer bytes (an actual out-of-bounds read on
// real hardware). That's not something worth reproducing in a
// memory-safe port, so SpeedUpMusic() just leaves main_tempo unchanged
// for those songs instead of reading garbage.
static const uint8_t speedup_index[8] = {
    0x07, // GHZ
    0x72, // LZ
    0x73, // MZ
    0x26, // SLZ
    0x15, // SYZ
    0x08, // SBZ
    0xFF, // Invincibility
    0x05, // Extra Life
};

void SpeedUpMusic(void) {
    int index = sound_music.current_music_id - bgm_GHZ;
    if (index < 0 || index >= (int)(sizeof(speedup_index) / sizeof(speedup_index[0])))
        return; // Out of SpeedUpIndex's real range -- leave tempo alone (see comment above)
    sound_music.main_tempo = speedup_index[index];
    if (sound_music.tempo_timeout > sound_music.main_tempo)
        sound_music.tempo_timeout = sound_music.main_tempo;
}

void SlowDownMusic(void) {
    sound_music.main_tempo = sound_music.base_main_tempo;
    if (sound_music.tempo_timeout > sound_music.main_tempo)
        sound_music.tempo_timeout = sound_music.main_tempo;
}

void Sound_Pause(void) { sound_music.paused = 1; }
void Sound_Resume(void) { sound_music.paused = 0; }

void PlaySound(uint8_t id) {
    if (id == 0) {
        StopAllSound();
        return;
    }

    if (id >= SOUND_ID_FLAG_FIRST && id <= SOUND_ID_FLAG_LAST) {
        switch (id) {
            case 0xE0: FadeOutMusic(); break;
            case 0xE1: PlaySegaSound(); break;
            case 0xE2: SpeedUpMusic(); break;
            case 0xE3: SlowDownMusic(); break;
            case 0xE4: StopAllSound(); break;
        }
        return;
    }

    if (id >= SOUND_ID_MUSIC_FIRST && id <= SOUND_ID_MUSIC_LAST) {
        PlayMusic(id);
        return;
    }

    if (id < SOUND_ID_SFX_FIRST || id > SOUND_ID_SPECIAL_LAST)
        return; // Not a valid ID (matches the real driver's silent reject)

    uint8_t priority = sound_priorities[id - SOUND_ID_SFX_FIRST];
    if (priority < sound_sfx.current_priority)
        return; // Lower priority than what's already committed to play -- ignored
    sound_sfx.current_priority = priority;
    QueueSound2(id);
}

// ---------------------------------------------------------------------
// Per-channel byte-stream interpreter
// ---------------------------------------------------------------------

// -1 for FM/DAC channels (no PSG register writes -- ymfm isn't wired up
// yet, so those channels' data still advances correctly for timing/looping
// purposes, they just don't make sound).
static int PSGRegisterChannel(int channel_index) {
    if (channel_index >= SOUND_CHANNEL_PSG_BASE && channel_index < SOUND_CHANNEL_PSG_BASE + 4)
        return channel_index - SOUND_CHANNEL_PSG_BASE;
    return -1;
}

static void SilenceIfPSG(SN76489 *psg, int psg_chan) {
    if (psg_chan >= 0)
        PSG_SetAttenuation(psg, psg_chan, 0x0F);
}

// Real driver's FinishTrackUpdate: every track update that isn't a
// smpsNoAttack ($E7) re-arms modulation from its own stored raw parameter
// bytes, but only if modulation is currently active ($F0 auto-enables it,
// $F4 turns it back off without forgetting the parameters).
static void ResetModulationIfActive(SoundChannel *ch) {
    if (!ch->mod_active || !ch->modulation_ptr)
        return;
    const uint8_t *p = ch->modulation_ptr;
    ch->modulation_wait = p[0];
    ch->modulation_speed = p[1];
    ch->modulation_delta = (int8_t)p[2];
    ch->modulation_steps = (uint8_t)(p[3] >> 1); // halved, same as $F0's own initial store
    ch->modulation_val = 0;
}

// Real driver's DoModulation: a triangle-wave stepper gated by a per-note
// wait, then a re-arming speed countdown, then step count before flipping
// direction. Note the asymmetry versus ResetModulationIfActive above: the
// *first* half-cycle after a note-on uses the halved step count, but every
// later reload here uses the raw, unhalved byte straight from the stream --
// that's not a bug, it's what the real disassembly does (loc_71DFE reads
// 3(a0) directly, never re-halving it).
static void StepModulation(SoundChannel *ch) {
    if (!ch->mod_active || !ch->modulation_ptr)
        return;
    if (ch->modulation_wait > 0) {
        ch->modulation_wait--;
        return;
    }
    if (--ch->modulation_speed != 0)
        return;
    const uint8_t *p = ch->modulation_ptr;
    ch->modulation_speed = p[1];
    if (ch->modulation_steps != 0) {
        ch->modulation_steps--;
        ch->modulation_val = (int16_t)(ch->modulation_val + ch->modulation_delta);
    } else {
        ch->modulation_steps = p[3];
        ch->modulation_delta = (int8_t)(-ch->modulation_delta);
    }
}

static int debug_isolate_voice = -1; // see Sound_DebugIsolateVoice's own comment in Sound.h

static void TickChannel(SoundChipSet *cs, int channel_index) {
    SoundChannel *ch = &cs->channels[channel_index];
    if (!ch->active)
        return;

    int psg_chan = PSGRegisterChannel(channel_index);
    // $F3 (smpsPSGform) redirects this track to drive the real hardware
    // noise channel (SN76489 channel 3) instead of its own tone channel --
    // psg_reg_chan is which register slot to actually write to, while
    // psg_chan stays the track's own logical slot (needed for $F3's own
    // "only PSG3" gate).
    int psg_reg_chan = (psg_chan >= 0 && ch->psg_noise) ? 3 : psg_chan;
    int is_dac = (channel_index == SOUND_CHANNEL_DAC);
    int is_fm = (channel_index >= SOUND_CHANNEL_FM_BASE && channel_index < SOUND_CHANNEL_FM_BASE + 6);

    // Note-fill release tail: gate off early, before the command stream
    // advances to the next byte. Matches the real driver's
    // NoteTimeoutUpdate exactly: note_timeout==0 means the feature is
    // inactive (never decremented, never fires) -- it is NOT "already
    // expired, fire now". Only a nonzero note_timeout counts down, and
    // firing happens the instant it reaches 0 from that countdown, not on
    // every subsequent frame it happens to read as 0.
    if (ch->note_timeout > 0) {
        ch->note_timeout--;
        if (ch->note_timeout == 0 && ch->key_on) {
            ch->key_on = 0;
            SilenceIfPSG(&cs->psg, psg_reg_chan);
            if (is_fm)
                FM_KeyOnOff(cs->fm, channel_index, 0);
        }
    }

    // Volume envelope: steps every governed tick regardless of whether a
    // new note/command is being read this tick, same as the real driver's
    // PSGUpdateVolFX running independently of PSGUpdateTrack.
    if (psg_chan >= 0 && ch->key_on) {
        int vol = PSGStepEnvelope(ch);
        if (vol >= 0)
            PSG_SetAttenuation(&cs->psg, psg_reg_chan, ch->debug_muted ? 0x0F : (uint8_t)vol);
    }

    if (ch->duration_timeout > 0) {
        ch->duration_timeout--;
        // Modulation (vibrato/pitch-bend, $F0-$F1/$F4) only steps while a
        // note is being held, not on the frame it's (re)triggered -- matches
        // the real driver only calling DoModulation from its "notegoing"
        // path, never from the fresh-note-on path.
        if (ch->mod_active) {
            StepModulation(ch);
            if (ch->key_on) {
                if (psg_chan >= 0 && !ch->psg_noise) {
                    int period = (int)PSGPeriodForNote(ch->note_index) + ch->modulation_val;
                    if (period < 1)
                        period = 1;
                    if (period > 1023)
                        period = 1023;
                    PSG_SetTonePeriod(&cs->psg, psg_reg_chan, (uint16_t)period);
                } else if (is_fm) {
                    FM_SetFrequency(cs->fm, channel_index, ch->note_index, ch->modulation_val);
                }
            }
        }
        return;
    }

    while (ch->active && ch->duration_timeout == 0) {
        uint8_t b = *ch->data_ptr++;

        if (b == 0xE7) { // smpsNoAttack -- hold the current pitch, don't retrigger
            uint8_t raw = *ch->data_ptr;
            if (raw < 0x80) {
                // "Dividing timing": the raw duration byte from the stream
                // gets multiplied here, once, and the *multiplied* value is
                // what's kept/reused (SavedDuration is post-multiplication
                // on real hardware) -- matches the real driver's
                // "move.b SMPS_Track.SavedDuration,DurationTimeout" reuse
                // path, which does no further multiplying.
                int scaled = (int)raw * (int)ch->tempo_divider;
                ch->saved_duration = (uint8_t)(scaled > 0xFF ? 0xFF : scaled);
                ch->data_ptr++;
            }
            uint8_t dur = ch->saved_duration ? ch->saved_duration : 1;
            ch->duration_timeout = dur;
            // Note fill ($E8) is the number of frames the note is *allowed
            // to play*, not a release-tail subtracted from duration -- 0
            // means disabled, and must stay exactly 0 (not fall back to
            // dur), or the note-timeout gate above would still fire right
            // at the note's own natural end and cut off the PSG envelope's
            // decay tail even with note-fill "off". Matches FinishTrackUpdate's
            // unconditional "move.b NoteTimeoutMaster,NoteTimeout".
            ch->note_timeout = ch->note_timeout_master;
            ch->key_on = 1;
            SOUND_TRACE("ch%d: $E7 no-attack dur=%u\n", channel_index, dur);
            break;
        }

        if (b < 0x80) { // Bare duration, no note prefix: keep playing the
                         // currently-held note/pitch (no new frequency write --
                         // this does NOT retrigger the note), just change how
                         // long for.
            int scaled = (int)b * (int)ch->tempo_divider;
            ch->saved_duration = (uint8_t)(scaled > 0xFF ? 0xFF : scaled);
            uint8_t dur = ch->saved_duration ? ch->saved_duration : 1;
            ch->duration_timeout = dur;
            ch->note_timeout = ch->note_timeout_master; // see the real note-on branch below for why not `dur`
            // FinishTrackUpdate resets VolEnvIndex unconditionally on every
            // track update except smpsNoAttack ($E7) -- including this bare-
            // duration case. The pitch doesn't retrigger, but the volume
            // envelope DOES restart from the top here, producing a fresh
            // attack-decay swell at the same held pitch -- this is what
            // actually produces LZ's "echo" character on a held note broken
            // into several bare-duration continuation bytes (e.g.
            // "nE6, $0C, $0C, $0C, $06"): four envelope swells at one
            // pitch, not four separate note-on events.
            if (psg_chan >= 0)
                ch->vol_env_index = 0;
            ResetModulationIfActive(ch);
            SOUND_TRACE("ch%d: bare-duration raw=$%02X dur=%u\n", channel_index, b, dur);
            break;
        }

        if (b < 0xE0) { // Note byte: 0x80 = rest, else pitch relative to C0
            uint8_t raw = *ch->data_ptr;
            if (raw < 0x80) {
                int scaled = (int)raw * (int)ch->tempo_divider;
                ch->saved_duration = (uint8_t)(scaled > 0xFF ? 0xFF : scaled);
                ch->data_ptr++;
            }
            uint8_t dur = ch->saved_duration ? ch->saved_duration : 1;
            ch->duration_timeout = dur;

            if (b == 0x80) {
                ch->key_on = 0;
                SilenceIfPSG(&cs->psg, psg_reg_chan);
                if (is_fm)
                    FM_KeyOnOff(cs->fm, channel_index, 0);
                ch->note_timeout = ch->note_timeout_master; // see the note-on branch below for why not `dur`
                if (psg_chan >= 0)
                    ch->vol_env_index = 0; // FinishTrackUpdate resets this unconditionally, rests included
                ResetModulationIfActive(ch);
                SOUND_TRACE("ch%d: rest dur=%u\n", channel_index, dur);
            } else if (is_dac) {
                // Sonic1PC's own EXTENDED DAC scheme (per your direction) --
                // a superset of real Sonic 1's own note-byte layout,
                // incorporating Sonic 2's four extra percussion samples
                // (Scratch/Clap/Tom/Bongo) at renumbered slots so both fit
                // without collision (real Sonic 2 uses $83 for Clap and $87
                // for Bongo, which real Sonic 1 already uses for Timpani and
                // the "SEGA!" PCM clip respectively -- moved to $85/$92
                // here instead). $81-$87 are the 7 base DPCM samples (Kick/
                // Snare/Timpani/Scratch/Clap/Tom/Bongo, one DAC_Trigger
                // each). $88-$91 are pitch-shifted variants of Timpani/Tom/
                // Bongo -- same DPCM data as their own base sample, just
                // played back at a different rate (real driver's
                // DACUpdateTrack: any sample byte with bit 3 set gets
                // rewritten to its base sample after stashing a rate into
                // zTimpani_Pitch -- see timpani_variant_rate_hz above for
                // the real per-variant Hz values transcribed directly from
                // Sonic 1's own DAC_sample_rate/byte_71CC4 table, and
                // tom_variant_rate_hz/bongo_variant_rate_hz for Tom/Bongo,
                // derived from Sonic 2's dac_sample_metadata scale factors
                // -- base_rate*scale -- against their own confirmed base
                // rates). $92 is the "SEGA!" PCM clip (was $87 in real
                // Sonic 1). Pitch overrides persist across other samples
                // playing in between, same as real hardware's "this
                // affects the raw pitch of the base sample, meaning it will
                // use this value from then on" -- see
                // cs->dac_pitch_override_sample/rate. NOTE: this project
                // models that persistence with a single shared
                // override-sample/rate pair (matching real hardware's own
                // apparent single-register redirect-and-restash mechanism,
                // as best understood from the disassembly), so interleaving
                // Timpani/Tom/Bongo variants (e.g. a Bongo variant right
                // after a Timpani one) means only the MOST RECENT family's
                // override is remembered -- not independently verified
                // against real hardware for the 3-family case, since real
                // Sonic 1 only ever had Timpani to test this with.
                if (b == 0x92) {
                    PlaySegaSound_Trigger(cs);
                } else if (b >= 0x88 && b <= 0x8B) {
                    cs->dac_pitch_override_sample = DAC_SAMPLE_TIMPANI;
                    cs->dac_pitch_override_rate = timpani_variant_rate_hz[b - 0x88];
                    cs->dac_timpani_variant = b - 0x88;
                    DAC_Trigger(cs, DAC_SAMPLE_TIMPANI);
                } else if (b >= 0x8C && b <= 0x8E) {
                    cs->dac_pitch_override_sample = DAC_SAMPLE_TOM;
                    cs->dac_pitch_override_rate = tom_variant_rate_hz[b - 0x8C];
                    cs->dac_timpani_variant = -1;
                    DAC_Trigger(cs, DAC_SAMPLE_TOM);
                } else if (b >= 0x8F && b <= 0x91) {
                    cs->dac_pitch_override_sample = DAC_SAMPLE_BONGO;
                    cs->dac_pitch_override_rate = bongo_variant_rate_hz[b - 0x8F];
                    cs->dac_timpani_variant = -1;
                    DAC_Trigger(cs, DAC_SAMPLE_BONGO);
                } else if (b >= SND_dKick && b <= SND_dKick + 6) { // $81-$87
                    cs->dac_timpani_variant = -1;
                    DAC_Trigger(cs, b - SND_dKick);
                }
                ch->key_on = 1;
                ch->note_timeout = dur;
            } else {
                int note_index = (int)(b - 0x81) + ch->transpose;
                ch->note_index = note_index;
                ResetModulationIfActive(ch); // reset before this note's first frequency write, so modulation_val is 0 for it
                if (psg_chan >= 0) {
                    // A noise-redirected track's own notes DO still set a
                    // period -- just not its own (real hardware has no
                    // period register on the noise channel at all). Real
                    // driver's PSGUpdateFreq: when VoiceControl marks a
                    // track as noise-redirected ($E0), it overrides the
                    // channel-select bits to $C0 ("PSG channel 2") before
                    // writing the frequency, unconditionally -- i.e. it
                    // hijacks PSG channel 2's own period register. That's
                    // exactly the register SN76489's noise generator reads
                    // from in "sync" mode (shift_rate==3, which $F3's real
                    // $E7 usage always selects -- see SN76489.c), so this is
                    // how a composer actually controls noise pitch: not via
                    // a noise-specific register (there isn't one), but by
                    // repurposing PSG2's period register through whichever
                    // track got redirected. Skipping this (as this code
                    // used to) leaves the noise generator reading whatever
                    // stale/unrelated value PSG2's own track last set,
                    // independent of the noise track's own authored notes
                    // -- audibly, every noise hit ends up the same pitch
                    // regardless of what note is written.
                    PSG_SetTonePeriod(&cs->psg, ch->psg_noise ? 2 : psg_reg_chan, PSGPeriodForNote(note_index));
                    PSG_SetAttenuation(&cs->psg, psg_reg_chan, ch->debug_muted ? 0x0F : ch->volume);
                    ch->vol_env_index = 0; // Every new note restarts its volume envelope from the top
                } else if (is_fm) {
                    FM_SetFrequency(cs->fm, channel_index, note_index, ch->modulation_val);
                    if (debug_isolate_voice < 0 || ch->voice_index == debug_isolate_voice)
                        FM_KeyOnOff(cs->fm, channel_index, 1);
                }
                ch->key_on = 1;
                // Note fill ($E8) is the number of frames the note is
                // *allowed to play*, not a release-tail subtracted from
                // duration -- must stay exactly note_timeout_master (0 =
                // disabled, never fires), not fall back to `dur`, or the
                // gate above would fire right at the note's own natural
                // end even with note-fill "off", cutting off the PSG
                // envelope before it can restart on the next bare-duration
                // continuation. Matches FinishTrackUpdate's unconditional
                // "move.b NoteTimeoutMaster,NoteTimeout".
                ch->note_timeout = ch->note_timeout_master;
                SOUND_TRACE("ch%d: note byte=$%02X note_index=%d dur=%u vol=%u transpose=%d\n", channel_index, b,
                            note_index, dur, ch->volume, (int)ch->transpose);
            }
            break;
        }

        // Coordination flags ($E0-$FF, except $E7 handled above)
        switch (b) {
            case 0xE0: { // smpsPan -- FM and DAC; no-op for PSG (can't set panning)
                uint8_t v = *ch->data_ptr++;
                if (is_fm)
                    FM_SetPan(cs->fm, channel_index, v);
                else if (is_dac)
                    cs->dac_pan = v & 0xC0;
                break;
            }
            case 0xE1: ch->transpose = (int8_t)*ch->data_ptr++; break; // smpsDetune -- SETS the channel key displacement (same field the header's pitch byte and $E9 use)
            case 0xE2: ch->data_ptr++; break; // smpsNop -- argument byte, no effect
            case 0xE3: // smpsReturn
                if (ch->return_sp > 0)
                    ch->data_ptr = ch->return_stack[--ch->return_sp];
                break;
            case 0xE4: // smpsFade -- stops this track; fade-in-to-previous-song isn't implemented
                ch->active = 0;
                SilenceIfPSG(&cs->psg, psg_reg_chan);
                if (is_fm)
                    FM_KeyOnOff(cs->fm, channel_index, 0);
                return;
            case 0xE5: ch->tempo_divider = *ch->data_ptr++; break; // smpsChanTempoDiv -- per-track duration multiplier override
            case 0xE6: { // smpsAlterVol -- designed for FM, effective immediately
                int max = is_fm ? 0x7F : 0x0F; // FM TL is 7-bit; PSG attenuation only uses the low nibble
                int v = (int)ch->volume + (int8_t)*ch->data_ptr++;
                ch->volume = (uint8_t)(v < 0 ? 0 : (v > max ? max : v));
                if (is_fm)
                    FM_ApplyVolume(cs->fm, ch, channel_index);
                break;
            }
            case 0xE8: ch->note_timeout_master = *ch->data_ptr++; break; // smpsNoteFill
            case 0xE9: ch->transpose = (int8_t)(ch->transpose + (int8_t)*ch->data_ptr++); break; // smpsChangeTransposition
            case 0xEA: cs->main_tempo = *ch->data_ptr++; break; // smpsSetTempoMod -- chip-set-wide, music only
            case 0xEB: { // smpsSetTempoDiv -- chip-set-wide duration multiplier, propagates to every track's own copy
                uint8_t v = *ch->data_ptr++;
                cs->duration_mult = v;
                for (int i = 0; i < SOUND_CHANNELS; i++)
                    cs->channels[i].tempo_divider = v;
                break;
            }
            case 0xEC: { // smpsPSGAlterVol -- designed for PSG; for FM only takes effect on the next $EF voice load
                int max = is_fm ? 0x7F : 0x0F;
                int v = (int)ch->volume + (int8_t)*ch->data_ptr++;
                ch->volume = (uint8_t)(v < 0 ? 0 : (v > max ? max : v));
                break;
            }
            case 0xED: break; // smpsClearPush -- clears sfx_Push's "pushing block" game-state flag, unrelated to the sound engine
            case 0xEE: // smpsStopSpecial
                ch->active = 0;
                SilenceIfPSG(&cs->psg, psg_reg_chan);
                if (is_fm)
                    FM_KeyOnOff(cs->fm, channel_index, 0);
                return;
            case 0xEF: { // smpsFMvoice / smpsSetvoice
                uint8_t voice_index = *ch->data_ptr++;
                ch->voice_index = voice_index;
                if (is_fm && cs->voice_bank)
                    FM_LoadVoice(cs->fm, ch, channel_index, cs->voice_bank + (size_t)voice_index * 25);
                break;
            }
            case 0xF0: // smpsModSet -- also immediately activates modulation on real
                       // hardware (cfModulation's own bset #3), no separate $F1 needed
                ch->modulation_ptr = ch->data_ptr; // raw stream bytes, reread later by StepModulation's step-count reload
                ch->modulation_wait = *ch->data_ptr++;
                ch->modulation_speed = *ch->data_ptr++;
                ch->modulation_delta = (int8_t)*ch->data_ptr++;
                ch->modulation_steps = (uint8_t)(*ch->data_ptr++ >> 1); // halved, matches the real driver's lsr.b #1
                ch->modulation_val = 0;
                ch->mod_active = 1;
                break;
            case 0xF1: ch->mod_active = 1; break; // smpsModOn -- re-enable without resetting stored parameters
            case 0xF2: // smpsStop
                ch->active = 0;
                SilenceIfPSG(&cs->psg, psg_reg_chan);
                if (is_fm)
                    FM_KeyOnOff(cs->fm, channel_index, 0);
                return;
            case 0xF3: { // smpsPSGform -- irreversibly redirects this track to drive the real
                         // hardware noise channel (SN76489 channel 3) instead of its own tone
                         // channel. Only safe on PSG3 ("without bugs" per the real driver's own
                         // documentation), and only when the 4th PSG slot isn't already dedicated
                         // to noise on its own (psg_count==4).
                uint8_t v = *ch->data_ptr++;
                if (psg_chan == 2 && cs->psg_count < 4) {
                    SN76489_Write(&cs->psg, v); // v is already a valid $E0-$E7 noise-control byte
                    ch->psg_noise = 1;
                }
                break;
            }
            case 0xF4: ch->mod_active = 0; break; // smpsModOff
            case 0xF5: ch->voice_index = *ch->data_ptr++; break; // smpsPSGvoice
            case 0xF6: { // smpsJump
                const uint8_t *word_pos = ch->data_ptr;
                int16_t rel = (int16_t)ReadWord(word_pos);
                ch->data_ptr = word_pos + 1 + rel;
                break;
            }
            case 0xF7: { // smpsLoop
                uint8_t index = *ch->data_ptr++;
                uint8_t loops = *ch->data_ptr++;
                const uint8_t *word_pos = ch->data_ptr;
                int16_t rel = (int16_t)ReadWord(word_pos);
                const uint8_t *target = word_pos + 1 + rel;
                ch->data_ptr = word_pos + 2;
                if (index < 3) {
                    if (ch->loop_counters[index] == 0)
                        ch->loop_counters[index] = loops;
                    ch->loop_counters[index]--;
                    if (ch->loop_counters[index] != 0)
                        ch->data_ptr = target;
                }
                break;
            }
            case 0xF8: { // smpsCall
                const uint8_t *word_pos = ch->data_ptr;
                int16_t rel = (int16_t)ReadWord(word_pos);
                const uint8_t *target = word_pos + 1 + rel;
                if (ch->return_sp < 2)
                    ch->return_stack[ch->return_sp++] = word_pos + 2;
                ch->data_ptr = target;
                break;
            }
            case 0xF9: break; // smpsMaxRelRate -- FM-only, no-op for PSG
            default: ch->data_ptr++; break; // Unknown/reserved flag -- consume one arg byte defensively
        }
    }
}

// ---------------------------------------------------------------------
// Frame tick
// ---------------------------------------------------------------------

static void TickChipSet(SoundChipSet *cs) {
    if (cs->paused) // Sound_Pause() -- only sound_music ever sets this
        return;

    // Shared tempo governor -- matches the real driver's TempoWait exactly
    // (s1.sounddriver.asm): every track's DurationTimeout gets an
    // unconditional "addq.b #1", regardless of whether that track is
    // active, and as a plain 8-bit add it wraps on overflow (255+1->0)
    // rather than clamping. Both properties matter: skipping inactive
    // channels or clamping at 0xFF (as this used to do) makes a channel
    // that ever hits max duration diverge permanently from real hardware's
    // timing by a fixed offset from that point on -- exactly the kind of
    // per-channel desync (not a gradual drift) this was found to cause.
    //
    // This must run BEFORE the queue is dispatched below -- the real
    // driver's main loop (UpdateMusic) does subq.b/TempoWait first, and
    // only afterward calls CycleSoundQueue/PlaySoundID to load any newly
    // queued song/SFX. Doing it in the other order (as this used to) means
    // a freshly-loaded song's very first frame either does or doesn't get
    // an extra tempo correction depending purely on our call order, not on
    // anything the song data says -- a real source of startup-frame skew
    // for whichever song/SFX happens to get dispatched this tick.
    if (cs->main_tempo != 0) {
        if (cs->tempo_timeout > 0)
            cs->tempo_timeout--;
        if (cs->tempo_timeout == 0) {
            cs->tempo_timeout = cs->main_tempo;
            for (int i = 0; i < SOUND_CHANNELS; i++)
                cs->channels[i].duration_timeout++; // uint8_t: wraps naturally, matching addq.b
        }
    }

    DispatchQueue(cs, SOUND_QUEUE_NORMAL);
    DispatchQueue(cs, SOUND_QUEUE_SPECIAL);

    for (int i = 0; i < SOUND_CHANNELS; i++)
        TickChannel(cs, i);

    // Once everything in this chip set has finished playing, release any
    // priority claim -- matches v_sndprio only mattering while something's
    // actually still occupying the channels it was set for.
    if (cs->current_priority != 0) {
        int any_active = 0;
        for (int i = 0; i < SOUND_CHANNELS; i++)
            if (cs->channels[i].active)
                any_active = 1;
        if (!any_active)
            cs->current_priority = 0;
    }
}

void Sound_Frame(void) {
    TickChipSet(&sound_music);
    TickChipSet(&sound_sfx);
    sound_trace_frame++;
}

// `out` is interleaved stereo (2*count entries: L,R,L,R,...), additive
// (callers must clear it first). Real SN76489 hardware has no panning
// capability at all, so PSG is generated into a mono scratch buffer and
// duplicated equally into both channels.
#define SOUND_SCRATCH_MAX 4096

void Sound_Generate(int32_t *out, uint32_t count, uint32_t sample_rate) {
    static int32_t psg_scratch[SOUND_SCRATCH_MAX];
    uint32_t n = count < SOUND_SCRATCH_MAX ? count : SOUND_SCRATCH_MAX;
    memset(psg_scratch, 0, n * sizeof(int32_t));
    if (!getenv("SONIC_FM_ONLY") && debug_isolate_voice < 0) {
        SN76489_Generate(&sound_music.psg, psg_scratch, n, sample_rate, SOUND_PSG_CLOCK);
        SN76489_Generate(&sound_sfx.psg, psg_scratch, n, sample_rate, SOUND_PSG_CLOCK);
    }
    for (uint32_t i = 0; i < n; i++) {
        out[2 * i + 0] += psg_scratch[i];
        out[2 * i + 1] += psg_scratch[i];
    }

    // Multiplies FM's contribution to the mix by a fixed gain before adding
    // it in -- real TL/attenuation register math is unaffected, this only
    // scales FM's overall level in the final mix relative to PSG/DAC, which
    // is otherwise measurably ~5-6x quieter than PSG+DAC even when playing
    // fully correct, byte-verified voice data (see the RMS comparison this
    // was based on). SONIC_FM_GAIN=N overrides the default for A/B testing.
    const char *fm_gain_env = getenv("SONIC_FM_GAIN");
    int fm_gain = fm_gain_env ? atoi(fm_gain_env) : 10;
    if (fm_gain != 1) {
        static int32_t fm_scratch[2 * SOUND_SCRATCH_MAX];
        uint32_t fn = count < SOUND_SCRATCH_MAX ? count : SOUND_SCRATCH_MAX;
        memset(fm_scratch, 0, 2 * fn * sizeof(int32_t));
        YM2612_Generate(sound_music.fm, fm_scratch, fn, sample_rate, SOUND_FM_CLOCK);
        YM2612_Generate(sound_sfx.fm, fm_scratch, fn, sample_rate, SOUND_FM_CLOCK);
        for (uint32_t i = 0; i < 2 * fn; i++)
            out[i] += fm_scratch[i] * fm_gain;
    } else {
        YM2612_Generate(sound_music.fm, out, count, sample_rate, SOUND_FM_CLOCK);
        YM2612_Generate(sound_sfx.fm, out, count, sample_rate, SOUND_FM_CLOCK);
    }
    if (!getenv("SONIC_FM_ONLY") && debug_isolate_voice < 0) {
        // sound_sfx has no DAC-sample track -- real hardware's SFX RAM
        // layout (v_sfx_track_ram) never allocates one (only FM3-5/PSG1-3),
        // and SFXChannelIndex() never routes any SFX channel ID to
        // SOUND_CHANNEL_DAC either, so sound_sfx.channels[DAC] can never
        // become active. DAC_Generate(&sound_sfx,...) was a guaranteed-inert
        // call every frame; PCM_Generate stays for both, since the separate
        // "SEGA!" PCM clip ($E1/$87) can legitimately trigger from either
        // chip set's own track data.
        DAC_Generate(&sound_music, out, count, sample_rate);
        PCM_Generate(&sound_music, out, count, sample_rate);
        PCM_Generate(&sound_sfx, out, count, sample_rate);
    }
}

// Debug/tooling only (ParadoxComposer): raw pointer to a song/SFX's
// compiled data by ID, NULL if unused. Lets a standalone tool read the
// voice bank and header directly without going through PlayMusic/LoadMusic.
const uint8_t *Sound_DebugGetSongData(uint8_t id) { return sound_table[id]; }

// Debug/tooling only (ParadoxComposer): play an arbitrary, not-yet-baked-in
// compiled song/SFX buffer -- e.g. one just produced by json_to_header.py
// from an edited .jsonc, with no ID in sound_table at all. PlayMusic/
// PlaySound only accept an ID looked up in that compile-time table, so this
// bypasses the lookup and calls straight into the same LoadMusic/LoadSFX
// logic DispatchQueue uses, letting the tool audition arbitrary song bytes
// through the real sequencer without recompiling the game.
void Sound_DebugPlayRawSong(const uint8_t *data, uint8_t is_sfx) {
    if (is_sfx)
        LoadSFX(&sound_sfx, data);
    else
        LoadMusic(&sound_music, data, 0);
}

void Sound_DebugSetChannelMuted(int channel_index, uint8_t muted) {
    if (channel_index < 0 || channel_index >= SOUND_CHANNELS)
        return;
    sound_music.channels[channel_index].debug_muted = muted;
}

// Dedicated scratch chip-set for standalone DAC preview -- DAC_Trigger/
// DAC_Generate only ever touch a SoundChipSet's own dac_*/timpani_pitch_rate
// fields, so a zero-initialized instance used for nothing else is safe.
// dac_pan defaults to 0 (both channels off, i.e. silent) like any other
// zero-initialized SoundChipSet -- set explicitly to center below, same
// reasoning as YM2612_FMCore's own pan-default fix (a caller with no pan
// control of its own should still be audible).
static SoundChipSet dac_preview_chipset;
static int dac_preview_pan_initialized = 0;

static void EnsureDacPreviewPanInitialized(void) {
    if (dac_preview_pan_initialized)
        return;
    dac_preview_chipset.dac_pan = 0xC0;
    // -1, not the zero-initialized (static storage) default -- 0 is a real
    // DAC_SAMPLE_KICK, which would otherwise make the very first preview
    // ever triggered incorrectly think an override applies (matching the
    // sound_music/sound_sfx init in Sound_Init).
    dac_preview_chipset.dac_pitch_override_sample = -1;
    dac_preview_pan_initialized = 1;
}

void Sound_DebugPreviewDacSample(int sampleId) {
    EnsureDacPreviewPanInitialized();
    // Each direct-sample preview is independent -- doesn't inherit a
    // pitch-variant override from a previous Sound_DebugPreviewDacVariant
    // call, same as dac_timpani_variant being reset here too.
    dac_preview_chipset.dac_pitch_override_sample = -1;
    dac_preview_chipset.dac_timpani_variant = -1;
    DAC_Trigger(&dac_preview_chipset, sampleId);
}

void Sound_DebugPreviewDacVariant(int sampleId, int variant) {
    uint32_t rate;
    if (sampleId == DAC_SAMPLE_TIMPANI && variant >= 0 && variant < 4) {
        rate = timpani_variant_rate_hz[variant];
    } else if (sampleId == DAC_SAMPLE_TOM && variant >= 0 && variant < 3) {
        rate = tom_variant_rate_hz[variant];
    } else if (sampleId == DAC_SAMPLE_BONGO && variant >= 0 && variant < 3) {
        rate = bongo_variant_rate_hz[variant];
    } else {
        return; // not a real family/variant combination -- nothing to trigger
    }
    EnsureDacPreviewPanInitialized();
    dac_preview_chipset.dac_pitch_override_sample = sampleId;
    dac_preview_chipset.dac_pitch_override_rate = rate;
    // Debug tooling only (SMPSInspector) -- still Timpani-specific by
    // design (see its own declaration comment in Sound.h); left at -1 for
    // Tom/Bongo previews since that inspector visualization doesn't cover
    // them.
    dac_preview_chipset.dac_timpani_variant = (sampleId == DAC_SAMPLE_TIMPANI) ? variant : -1;
    DAC_Trigger(&dac_preview_chipset, sampleId);
}

void Sound_DebugGenerateDacPreview(int32_t *out, uint32_t count, uint32_t sample_rate) {
    DAC_Generate(&dac_preview_chipset, out, count, sample_rate);
}

int Sound_DebugIsDacPreviewPlaying(void) { return dac_preview_chipset.dac_playing; }

void Sound_DebugIsolateVoice(int voice_index) {
    debug_isolate_voice = voice_index;
    // Only gating *new* note-on events left an already-ringing channel
    // audible until its own natural note-off -- switching targets live
    // would briefly bleed the old voice into the new selection. Force any
    // currently-active, non-matching FM channel silent immediately instead.
    if (voice_index < 0)
        return;
    SoundChipSet *sets[2] = {&sound_music, &sound_sfx};
    for (int s = 0; s < 2; s++) {
        for (int i = 0; i < 6; i++) {
            SoundChannel *fch = &sets[s]->channels[SOUND_CHANNEL_FM_BASE + i];
            if (fch->key_on && fch->voice_index != voice_index) {
                fch->key_on = 0;
                FM_KeyOnOff(sets[s]->fm, SOUND_CHANNEL_FM_BASE + i, 0);
            }
        }
    }
}
