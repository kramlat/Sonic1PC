// Alternate YM2612.h implementation backed by fmcore (../../fmcore/fm_voice.h)
// instead of ymfm -- see fmcore/fm_operator.h's own header comment for what
// fmcore is and isn't (musically correct, not cycle-accurate). Selected in
// place of YM2612.cpp (the ymfm-backed implementation, kept fully intact and
// still buildable) via CMakeLists.txt's SOUND_FM_BACKEND option -- both
// files implement the exact same YM2612.h C API, so nothing outside this
// file and its CMake wiring needs to know or care which one is active.
//
// Register decoding here mirrors Sound.c's own FM_LoadVoice/FM_WriteReg
// comments exactly (natural op*4 register addressing per operator, no
// hardware slot remap needed at the register-address level -- confirmed
// empirically against real ymfm output during the fmcore integration work,
// see the conversation this came out of for the verification trace) and
// FM_KeyOnOff (all bits4-7 = natural op1-op4 key state, channel decode
// matches YM2612_PeekKeyOn's own established pattern below).

#include "YM2612.h"

#include "../../fmcore/fm_voice.h"

#include <stdlib.h>
#include <string.h>

#define FM_CHANNEL_COUNT 6

struct YM2612 {
    FMVoice voice[FM_CHANNEL_COUNT];

    // Same shadow-register approach as the ymfm backend (YM2612.cpp) --
    // real hardware is write-only, and register writes here only ever
    // arrive one field at a time (e.g. just TL, just DT/MUL), while
    // FMOperator_SetParams needs the operator's full field set at once. The
    // shadow lets each write recombine the complete up-to-date set instead
    // of needing new partial-update setters on fm_operator.c's own API.
    uint8_t reg_shadow[2][0x100];
    uint8_t latched_addr[2];

    uint8_t keyon_mask; // decoded, persistent -- see YM2612_PeekKeyOn's own comment (Backend/YM2612.h)
    uint8_t pan[FM_CHANNEL_COUNT]; // top 2 bits of $B4+ch (L/R only -- AMS now lives on each FMVoice instead, see the $B4-$B6 write handler below; FMS/PMS is a supported-off fixed zero, see that handler's own comment)

    // Chip-wide LFO (register $22: bit3=enable, bits2-0=frequency select) --
    // genuinely shared by all 6 channels on real hardware (unlike AMS, which
    // is per-channel and lives on each FMVoice instead), so it's tracked
    // here and its current value passed into each FMVoice_Clock call rather
    // than duplicated per-voice.
    bool lfo_enabled;
    uint8_t lfo_freq_sel; // 0-7
    float lfo_phase;      // 0..1, free-running

    uint32_t tick_accum;
    int16_t last_sample_l, last_sample_r;
};

// Approximate LFO rates (Hz) for freq_sel 0-7 -- commonly cited real-hardware
// values (see e.g. Nemesis'/Maxim's YM2612 register notes); not re-derived
// from a cycle-accurate model, consistent with fmcore's own "musically
// correct, not bit-exact" approach (see fm_operator.h's header comment).
static const float LFO_FREQ_HZ[8] = {3.98f, 5.56f, 6.02f, 6.37f, 6.88f, 9.63f, 48.1f, 72.2f};

// port(0/1) + ch(0-2 within port) -> this project's flat 0-5 channel index,
// matching FMPortChannel's own convention in Sound.c (port*3+ch).
static int ChannelIndex(int port, int ch) { return port * 3 + ch; }

// Reassembles one operator's full parameter set from the shadow and pushes
// it to fm_operator.c -- called after ANY of that operator's 6 registers
// change, so all of them (including ones this particular write didn't
// touch) stay current. op_index (0-3) is the natural op*4 slot index,
// which is also directly the FMVoice ops[] index -- no remap (see this
// file's own top comment).
static void SyncOperator(YM2612 *chip, int port, int ch, int op_index) {
    uint8_t *shadow = chip->reg_shadow[port];
    int slot = op_index * 4;
    uint8_t dt_mul = shadow[0x30 + slot + ch];
    uint8_t tl = shadow[0x40 + slot + ch];
    uint8_t rs_ar = shadow[0x50 + slot + ch];
    uint8_t am_d1r = shadow[0x60 + slot + ch];
    uint8_t d2r_reg = shadow[0x70 + slot + ch];
    uint8_t d1l_rr = shadow[0x80 + slot + ch];
    uint8_t alg_fb = shadow[0xB0 + ch];

    uint8_t dt = (uint8_t)((dt_mul >> 4) & 7);
    uint8_t mul = (uint8_t)(dt_mul & 0xF);
    uint8_t rs = (uint8_t)((rs_ar >> 6) & 3);
    uint8_t ar = (uint8_t)(rs_ar & 0x1F);
    uint8_t am = (uint8_t)((am_d1r >> 7) & 1);
    uint8_t d1r = (uint8_t)(am_d1r & 0x1F);
    uint8_t d2r = (uint8_t)(d2r_reg & 0x1F);
    uint8_t d1l = (uint8_t)((d1l_rr >> 4) & 0xF);
    uint8_t rr = (uint8_t)(d1l_rr & 0xF);
    // Feedback is a per-CHANNEL register field that real hardware only ever
    // self-modulates operator 1 (natural op index 0) with -- never any other
    // operator -- confirmed against ymfm's own algorithm implementation
    // during this integration. Every other operator's fb stays 0.
    uint8_t fb = (op_index == 0) ? (uint8_t)((alg_fb >> 3) & 7) : 0;

    FMOperator_SetParams(&chip->voice[ChannelIndex(port, ch)].ops[op_index], dt, mul, rs, ar, am, d1r, d2r, d1l, rr,
                          tl, fb);
}

static void SyncAlgorithm(YM2612 *chip, int port, int ch) {
    uint8_t alg_fb = chip->reg_shadow[port][0xB0 + ch];
    // 4-bit algorithm: low 3 bits as always, bit7 as the high algorithm bit
    // ("0"=real algorithm 0-7, "1"=custom fmcore algorithm 8-15) -- see
    // Sound.c's FM_LoadVoice and fm_voice.c's own comments. Real hardware
    // ignores bit7 entirely; this encoding is fmcore's own, not a real
    // register field.
    uint8_t algorithm = (uint8_t)((((alg_fb >> 7) & 1) << 3) | (alg_fb & 7));
    FMVoice *voice = &chip->voice[ChannelIndex(port, ch)];
    for (int i = 0; i < FM_VOICE_OP_COUNT; i++)
        for (int j = 0; j <= FM_VOICE_OP_COUNT; j++)
            voice->connect[i][j] = FM_VOICE_ALGORITHM_CONNECT[algorithm][i][j];
    // Feedback (op0 only) is folded into TL/DT/etc via SyncOperator elsewhere,
    // but since $B0 carries both algorithm AND feedback, and feedback only
    // takes effect through op0's own params, refresh op0 here too in case
    // this write changed feedback without any of op0's other registers
    // changing in the same voice-load pass.
    SyncOperator(chip, port, ch, 0);
}

static void SyncFreq(YM2612 *chip, int port, int ch) {
    uint8_t *shadow = chip->reg_shadow[port];
    uint8_t hi = shadow[0xA4 + ch];
    uint8_t lo = shadow[0xA0 + ch];
    uint16_t fnum = (uint16_t)(((hi & 7) << 8) | lo);
    uint8_t block = (uint8_t)((hi >> 3) & 7);
    FMVoice_SetFreq(&chip->voice[ChannelIndex(port, ch)], fnum, block);
}

YM2612 *YM2612_Create(void) {
    YM2612 *chip = (YM2612 *)calloc(1, sizeof(YM2612));
    // chip->lfo_enabled stays false (calloc) -- Sonic 1's SMPS driver has no
    // coordination flag that writes $22 yet, so the LFO (and therefore AM,
    // even on voices with AMS set via smpsPan) stays silently inert during
    // real gameplay, matching real hardware's own power-on/never-enabled
    // state. fmcore_synth_main.cpp's jig turns AM's effect on directly via
    // FMVoice_SetAMS + its own local LFO instead, to make it audible for
    // testing without needing a real $22 write path.
    for (int i = 0; i < FM_CHANNEL_COUNT; i++) {
        FMVoice_Init(&chip->voice[i]);
        // Pan ($B4's top 2 bits, L/R output enable) defaults to center
        // rather than calloc's 0 (both channels off, i.e. silent) -- real
        // hardware's own power-on state IS silent, but Sound.c's SMPS
        // driver always writes this explicitly before a channel can ever
        // sound (FM_LoadVoice), so real gameplay never observes this
        // default either way. A caller that never writes $B4 at all (a
        // voice-preview keyboard with no pan control, e.g. ParadoxComposer)
        // should still be audible rather than silently muted on both sides.
        chip->pan[i] = 0xC0;
    }
    return chip;
}

void YM2612_Destroy(YM2612 *chip) { free(chip); }

void YM2612_Write(YM2612 *chip, uint32_t offset, uint8_t data) {
    int port = (int)(offset >> 1);
    if ((offset & 1) == 0) {
        chip->latched_addr[port] = data;
        return;
    }

    uint8_t reg = chip->latched_addr[port];
    chip->reg_shadow[port][reg] = data;

    if (reg == 0x28) {
        // Key-on/off: port is always irrelevant here (real hardware only
        // ever exposes $28 on port 0's address space) -- channel and
        // per-operator state are both encoded in `data` itself, not `reg`.
        // Channel decode matches YM2612_PeekKeyOn's own established pattern
        // (group*3 + local). Bits4-7 = natural op1-op4 key state.
        int fm_channel = (data & 3) + ((data & 4) ? 3 : 0);
        if (fm_channel < FM_CHANNEL_COUNT) {
            FMVoice *voice = &chip->voice[fm_channel];
            bool any_on = false;
            for (int op = 0; op < FM_VOICE_OP_COUNT; op++) {
                bool on = (data & (0x10 << op)) != 0;
                if (on) {
                    FMOperator_KeyOn(&voice->ops[op]);
                    any_on = true;
                } else {
                    FMOperator_KeyOff(&voice->ops[op]);
                }
            }
            if (any_on)
                chip->keyon_mask |= (uint8_t)(1 << fm_channel);
            else
                chip->keyon_mask &= (uint8_t)~(1 << fm_channel);
        }
        return;
    }

    if (reg == 0x22) {
        // Chip-wide, not per-channel -- see the YM2612 struct's own comment.
        chip->lfo_enabled = (data & 0x08) != 0;
        chip->lfo_freq_sel = data & 0x07;
        return;
    }

    int ch = reg % 4; // registers repeat every 4 addresses per port (channel 0/1/2 within that port); reg%4==3 is unused (no 4th channel per port)
    if (ch == 3)
        return; // not a valid channel row -- nothing to do

    if (reg >= 0x30 && reg <= 0x8F) {
        int op_index = (int)((reg - 0x30) / 4) % FM_VOICE_OP_COUNT; // op*4 spacing, see this file's top comment
        SyncOperator(chip, port, ch, op_index);
    } else if (reg >= 0xB0 && reg <= 0xB2) {
        SyncAlgorithm(chip, port, ch);
    } else if (reg >= 0xB4 && reg <= 0xB6) {
        int idx = ChannelIndex(port, ch);
        chip->pan[idx] = (uint8_t)(data & 0xC0);
        FMVoice_SetAMS(&chip->voice[idx], (uint8_t)((data >> 4) & 3)); // bits5-4 = AMS, wired to real LFO-driven amplitude modulation
        // bits2-0 (FMS/PMS, pitch modulation depth) are intentionally a
        // fixed off/zero, not "unimplemented" in the sense of a gap --
        // real hardware's own /IC reset pin already puts every register
        // (including this one) at a defined 0 on power-on (the Z80's own
        // boot code never touches the YM2612 at all -- its reset state
        // comes from /IC, not software), and Sonic 1's own SMPS driver
        // never writes anything else into these bits either -- FMS/PMS is
        // specifically a Sonic 3 driver feature (confirmed seen in use in
        // Flamedriver), not something the Sonic 1 sound engine this project
        // reproduces ever touches. So 0/off is the ONLY state this project
        // needs to reproduce. Whatever value data's low 3 bits carry is
        // still preserved in reg_shadow (YM2612_PeekReg) even though it's
        // not acted on here.
    } else if ((reg >= 0xA0 && reg <= 0xA2) || (reg >= 0xA4 && reg <= 0xA6)) {
        SyncFreq(chip, port, ch);
    }
    // $27 (timers/ch3 special mode), $2A/$2B (hardware DAC) are still
    // unimplemented -- confirmed via direct grep that Sound.c's own
    // FM_WriteReg call sites never touch them (this project's DAC/PCM
    // playback goes through a separate PC-side mixer, not the real chip's
    // built-in DAC channel), so there's nothing real for this backend to
    // reproduce for those two.
}

uint8_t YM2612_PeekReg(const YM2612 *chip, int port, uint8_t reg) { return chip->reg_shadow[port & 1][reg]; }
uint8_t YM2612_PeekKeyOn(const YM2612 *chip) { return chip->keyon_mask; }

void YM2612_Generate(YM2612 *chip, int32_t *out, uint32_t count, uint32_t sample_rate, uint32_t clock_rate) {
    uint32_t native_rate = clock_rate / 144; // matches fm_operator.c's own OP_UPDATE_RATE derivation (SOUND_FM_CLOCK/144)
    for (uint32_t i = 0; i < count; i++) {
        // Same box-filter anti-aliasing approach as the ymfm backend's own
        // YM2612_Generate (see YM2612.cpp) -- average every native-rate tick
        // that falls within this output sample's window instead of
        // nearest-neighbor decimation.
        int64_t sum_l = 0, sum_r = 0;
        uint32_t taken = 0;
        chip->tick_accum += native_rate;
        while (chip->tick_accum >= sample_rate) {
            chip->tick_accum -= sample_rate;

            // One shared LFO value per tick, computed once here rather than
            // once per channel below -- real hardware has exactly one
            // free-running LFO, not six independent ones (see the YM2612
            // struct's own comment). Triangle wave 0..1..0 over one period,
            // matching AM's commonly-described real-hardware shape (see
            // fm_voice.c's AMS_MAX_ATTEN comment for where the depth itself
            // gets applied, per channel).
            float lfo_unipolar = 0.0f;
            if (chip->lfo_enabled) {
                chip->lfo_phase += LFO_FREQ_HZ[chip->lfo_freq_sel] / (float)native_rate;
                if (chip->lfo_phase >= 1.0f)
                    chip->lfo_phase -= 1.0f;
                lfo_unipolar = chip->lfo_phase < 0.5f ? (2.0f * chip->lfo_phase) : (2.0f * (1.0f - chip->lfo_phase));
            }

            int32_t mix_l = 0, mix_r = 0;
            for (int ch = 0; ch < FM_CHANNEL_COUNT; ch++) {
                int16_t sample = FMVoice_Clock(&chip->voice[ch], lfo_unipolar);
                if (chip->pan[ch] & 0x80)
                    mix_l += sample;
                if (chip->pan[ch] & 0x40)
                    mix_r += sample;
            }
            chip->last_sample_l = (int16_t)(mix_l > 32767 ? 32767 : (mix_l < -32768 ? -32768 : mix_l));
            chip->last_sample_r = (int16_t)(mix_r > 32767 ? 32767 : (mix_r < -32768 ? -32768 : mix_r));
            sum_l += chip->last_sample_l;
            sum_r += chip->last_sample_r;
            taken++;
        }
        if (taken > 0) {
            out[2 * i + 0] += (int32_t)(sum_l / (int64_t)taken);
            out[2 * i + 1] += (int32_t)(sum_r / (int64_t)taken);
        } else {
            out[2 * i + 0] += chip->last_sample_l;
            out[2 * i + 1] += chip->last_sample_r;
        }
    }
}
