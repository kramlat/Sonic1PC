#include "fm_operator.h"

#include <math.h>

// ---------------------------------------------------------------------
// Log-sine / exp tables
// ---------------------------------------------------------------------
// Real YM2612 hardware doesn't compute sin() directly -- it looks up a
// quarter-wave log-sine table (attenuation in a log2 domain), adds the
// envelope's own attenuation (also log-domain, so combining the two is a
// simple add instead of a multiply), then converts the summed attenuation
// back to a linear sample via an exp table. That log-domain quantization is
// a real, audible part of the chip's character, not just an implementation
// detail -- so this computes the same tables mathematically at startup
// (log2/exp2) rather than either the exact hardcoded ROM values (not
// transcribed here yet) or a plain floating-point sin()*level shortcut.
#define SINE_BITS  10 // phase resolution: 1024 steps/cycle
#define SINE_SIZE  (1 << SINE_BITS)
#define EXP_BITS   12
#define EXP_SIZE   (1 << EXP_BITS)

static uint16_t sine_table[SINE_SIZE / 4]; // log2 attenuation, quarter wave only (rest by symmetry)
static uint16_t exp_table[EXP_SIZE];       // linear amplitude from log2 attenuation
static int tables_built = 0;

static void BuildTables(void) {
    if (tables_built)
        return;
    tables_built = 1;

    for (int i = 0; i < SINE_SIZE / 4; i++) {
        // x sweeps 0..(pi/2), sin(x) 1..~0 -- attenuation = -log2(sin(x)),
        // scaled so the table's fixed-point resolution matches the exp
        // table's own input domain.
        double x = (i + 0.5) * (3.14159265358979323846 / 2.0) / (SINE_SIZE / 4);
        double s = sin(x);
        if (s < 1e-6)
            s = 1e-6; // avoid log2(0) right at the table's tail end
        double atten = -log2(s) * (EXP_SIZE / 16.0); // scale factor chosen to fill EXP_SIZE's useful range
        if (atten > EXP_SIZE - 1)
            atten = EXP_SIZE - 1;
        sine_table[i] = (uint16_t)(atten + 0.5);
    }
    for (int i = 0; i < EXP_SIZE; i++) {
        double atten = (double)i / (EXP_SIZE / 16.0);
        double linear = exp2(-atten); // 0 attenuation -> 1.0, more attenuation -> smaller
        exp_table[i] = (uint16_t)(linear * 8191.0 + 0.5); // 13-bit linear output domain
    }
}

// Looks up one quarter-wave log-sine value for a full-cycle phase index
// (0..SINE_SIZE-1), applying the standard quadrant mirroring/sign real
// hardware uses (sine is symmetric within a cycle, so only one quarter's
// worth of table entries are needed).
static uint16_t SineLookup(uint32_t phase_index, int *negative) {
    uint32_t quadrant = (phase_index >> (SINE_BITS - 2)) & 3;
    uint32_t within = phase_index & (SINE_SIZE / 4 - 1);
    *negative = (quadrant >= 2);
    uint32_t idx = (quadrant & 1) ? (SINE_SIZE / 4 - 1 - within) : within;
    return sine_table[idx];
}

// ---------------------------------------------------------------------
// Envelope generator
// ---------------------------------------------------------------------
// The YM2612's own envelope generator (as in MAME's fm.cpp, matching Nuked-OPN2): a 10-bit attenuation
// (0 = loudest, 1023 = silent; 0.09375 dB per step, the same scale env_level always had here), stepped
// by a counter that ticks once every 3 output samples. Each operator rate (0-63: the register value x2,
// release x4+2, plus the key-scaling term) picks how often it steps (a shift: every 2^shift ticks) and by
// how much (an 8-step increment pattern from EG_INC). Attack curves toward 0 ("vol += (~vol * inc) >> 4"),
// rates 62-63 attack instantly, decay/sustain/release add the increment.
// (This used to be a smooth curve "calibrated against human perception" -- 4 ms to 3 s across the range
// -- which ran attacks and decays up to ten times faster than the chip at slow and moderate rates: notes
// died away far too quickly and slow swells were instant.)
#define EG_STEPS 8

static const uint8_t EG_INC[19 * EG_STEPS] = {
    0, 1, 0, 1, 0, 1, 0, 1,  // 0: rates 00-11 sub 0
    0, 1, 0, 1, 1, 1, 0, 1,  // 1: sub 1
    0, 1, 1, 1, 0, 1, 1, 1,  // 2: sub 2
    0, 1, 1, 1, 1, 1, 1, 1,  // 3: sub 3
    1, 1, 1, 1, 1, 1, 1, 1,  // 4: rate 12 sub 0
    1, 1, 1, 2, 1, 1, 1, 2,  // 5
    1, 2, 1, 2, 1, 2, 1, 2,  // 6
    1, 2, 2, 2, 1, 2, 2, 2,  // 7
    2, 2, 2, 2, 2, 2, 2, 2,  // 8: rate 13
    2, 2, 2, 4, 2, 2, 2, 4,  // 9
    2, 4, 2, 4, 2, 4, 2, 4,  // 10
    2, 4, 4, 4, 2, 4, 4, 4,  // 11
    4, 4, 4, 4, 4, 4, 4, 4,  // 12: rate 14
    4, 4, 4, 8, 4, 4, 4, 8,  // 13
    4, 8, 4, 8, 4, 8, 4, 8,  // 14
    4, 8, 8, 8, 4, 8, 8, 8,  // 15
    8, 8, 8, 8, 8, 8, 8, 8,  // 16: rate 15
    16, 16, 16, 16, 16, 16, 16, 16, // 17: instant attack (rates 62-63)
    0, 0, 0, 0, 0, 0, 0, 0,  // 18: infinite (rates 0-1)
};

// Which EG_INC row a rate (0-63) uses, and its shift (it steps every 2^shift counter ticks).
static uint8_t EgSelect(int rate) {
    if (rate < 4)
        return (uint8_t)(rate < 2 ? 18 : 0);   // YM2612: rates 0-1 never move
    if (rate < 8)
        return (uint8_t)(rate < 6 ? 0 : 2);    // rates 4-7 (Nemesis's measurements, as in MAME)
    if (rate < 48)
        return (uint8_t)(rate & 3);
    if (rate < 60)
        return (uint8_t)(4 + (rate - 48));     // rates 12-14: rows 4-15
    return 16;
}

static uint8_t EgShift(int rate) {
    int group = rate >> 2;
    return (uint8_t)(group < 12 ? 11 - group : 0);
}

// Key code (5 bits): block in bits 4-2, two "note" bits from F-number bits 10-7 (also used for detune).
static uint8_t KeyCode(const FMOperator *op) {
    static const uint8_t note[16] = {0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3};
    return (uint8_t)(((op->block & 7) << 2) | note[(op->fnum >> 7) & 0xF]);
}

// Effective 0-63 rate: 0 stays 0 ("never"); otherwise base + key scaling (kc >> (3 - RS)), capped at 63.
static int EgRate(int base, const FMOperator *op) {
    if (base == 0)
        return 0;
    int r = base + (KeyCode(op) >> (3 - op->rs));
    return r > 63 ? 63 : r;
}

// One envelope step for `rate` at counter value `cnt`: 0 when this rate doesn't step on this tick.
static int EgIncrement(int rate, uint32_t cnt) {
    uint8_t shift = EgShift(rate);
    if (cnt & ((1u << shift) - 1))
        return 0;
    return EG_INC[EgSelect(rate) * EG_STEPS + ((cnt >> shift) & 7)];
}

static void EnvelopeStep(FMOperator *op) {
    // The chip's envelope counter ticks once every 3 samples (12 bits, skipping 0). Every operator is
    // clocked exactly once per sample, so a per-operator copy started with the operator stays in lockstep
    // with every other one -- the same as the chip's single shared counter.
    if (++op->eg_div < 3)
        return;
    op->eg_div = 0;
    if (++op->eg_cnt >= 4096)
        op->eg_cnt = 1;
    uint32_t cnt = op->eg_cnt;

    int vol = (int)op->env_level;
    switch (op->env_state) {
    case FM_ENV_ATTACK: {
        int rate = EgRate(op->ar * 2, op);
        if (rate >= 62) {
            vol = 0;
        } else {
            int inc = EgIncrement(rate, cnt);
            if (inc)
                vol += ((~vol) * inc) >> 4;
        }
        if (vol <= 0) {
            vol = 0;
            op->env_state = FM_ENV_DECAY1;
        }
        break;
    }
    case FM_ENV_DECAY1: {
        int sl = (op->d1l == 15) ? 992 : op->d1l * 32;
        vol += EgIncrement(EgRate(op->d1r * 2, op), cnt);
        if (vol >= sl)
            op->env_state = FM_ENV_DECAY2;
        break;
    }
    case FM_ENV_DECAY2:
        vol += EgIncrement(EgRate(op->d2r * 2, op), cnt);
        if (vol > 1023)
            vol = 1023;
        break;
    case FM_ENV_RELEASE:
        vol += EgIncrement(EgRate(op->rr * 4 + 2, op), cnt);
        if (vol >= 1023) {
            vol = 1023;
            op->env_state = FM_ENV_OFF;
        }
        break;
    case FM_ENV_OFF:
        vol = 1023;
        break;
    }
    op->env_level = (uint32_t)vol;
}

// ---------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------

void FMOperator_Init(FMOperator *op) {
    BuildTables();
    op->phase = 0;
    op->phase_step = 0;
    op->env_state = FM_ENV_OFF;
    op->keyed = 0;
    op->env_level = 1023;
    op->eg_div = 0;
    op->eg_cnt = 0;
    op->dt = op->mul = op->rs = op->ar = op->am = op->d1r = op->d2r = op->d1l = op->rr = op->tl = op->fb = 0;
    op->fnum = 0;
    op->block = 0;
    op->fb_history[0] = op->fb_history[1] = 0;
}

// fnum/block -> phase-step, derived directly from the real chip's known
// frequency relationship (Fnote = fnum * clock * 2^(block-21) / 144) rather
// than transcribed from hardware docs -- worked backward from
// "phase_index = (phase >> 12) & 1023" in FMOperator_Clock, which means one
// full sine cycle is 2^22 units of `phase`, and the operator updates at
// clock/144 (SOUND_FM_CLOCK/144 in the rest of this project, see Sound.c):
//   phase_step = 2^22 * Fnote / (clock/144)
//              = 2^22 * [fnum * clock * 2^(block-21) / 144] / (clock/144)
//              = fnum * 2^(block+1)
// MUL (0 = x0.5, 1-15 = xN) folds in as (fnum<<block)*mul_x2, where
// mul_x2 = 2*MUL (or 1 for MUL=0) -- this replaces an earlier version of
// this function that had an extra stray >>1 >>1 (net /4), which was
// verified wrong: it measured as playing every note 2 octaves flat.
// Detune (DT) is the chip's own: a small offset from a fixed table, indexed by the key code (block and
// the top bits of the F-number), added to the phase increment BEFORE the multiplier. Values are in the
// chip's 20-bit phase units (YM2612/YM2151 dt_tab, as in MAME's fm.cpp; Nuked-OPN2 computes the same
// numbers). DT 1-3 detune up, 5-7 down by the same amounts, 0 and 4 not at all. (This used to be a
// percentage of the note -- ~1% per step, "tuned by ear" -- several times the chip's detune, which
// knocked modulators out of tune with their carriers: inharmonic, beating timbres.)
static const uint8_t DT_TABLE[4][32] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 8, 8},
    {1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 9, 10, 11, 12, 13, 14, 16, 16, 16, 16},
    {2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 20, 22, 22, 22, 22},
};
// Key code: block in bits 4-2, and two "note" bits from F-number bits 10-7.
static const uint8_t FN_NOTE[16] = {0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3};

// Shared by FMOperator_SetFreq and FMOperator_SetParams -- real hardware
// re-reads DT/MUL/fnum/block live on every operator update, no key-on
// latching involved, so either one changing must recompute phase_step
// immediately (previously only FMOperator_SetFreq did this, meaning a DT
// slider drag was silently inaudible until the next note-on -- exactly the
// kind of thing that makes a genuinely-present small pitch shift feel like
// nothing happened at all).
static void RecomputePhaseStep(FMOperator *op) {
    // Chip: inc = ((fnum << block) >> 1) + detune, then * MUL (MUL 0 = x0.5), in 20-bit phase units per
    // sample. Our phase has 2^22 units per cycle (4x), hence the final * 2 with mul_x2 = 2*MUL (or 1).
    uint32_t mul_x2 = op->mul ? (uint32_t)op->mul * 2 : 1;
    uint32_t keycode = ((uint32_t)(op->block & 7) << 2) | FN_NOTE[(op->fnum >> 7) & 0xF];
    int32_t detune = DT_TABLE[op->dt & 3][keycode];
    if (op->dt & 4)
        detune = -detune;
    int32_t inc = (int32_t)(((uint32_t)op->fnum << op->block) >> 1) + detune;
    if (inc < 0)
        inc = 0;
    op->phase_step = (uint32_t)inc * mul_x2 * 2;
}

void FMOperator_SetParams(FMOperator *op, uint8_t dt, uint8_t mul, uint8_t rs, uint8_t ar, uint8_t am, uint8_t d1r,
                           uint8_t d2r, uint8_t d1l, uint8_t rr, uint8_t tl, uint8_t fb) {
    op->dt = dt;
    op->mul = mul;
    op->rs = rs;
    op->ar = ar;
    op->am = am;
    op->d1r = d1r;
    op->d2r = d2r;
    op->d1l = d1l;
    op->rr = rr;
    op->tl = tl;
    op->fb = fb;
    RecomputePhaseStep(op); // DT/MUL apply immediately, matching real hardware's live (non-latched) register reads
}

void FMOperator_SetFreq(FMOperator *op, uint16_t fnum, uint8_t block) {
    op->fnum = fnum;
    op->block = block;
    RecomputePhaseStep(op);
}

void FMOperator_KeyOn(FMOperator *op) {
    // Like a real YM2612, keying on an operator that is already keyed on does nothing: only an
    // off->on transition starts a new attack. (The sound driver keys a channel off before each new
    // note, except after $E7 "no attack", which relies on exactly this to change pitch without a
    // new attack.)
    if (op->keyed)
        return;
    op->keyed = 1;
    op->phase = 0; // real hardware does NOT reset phase on key-on for most cases, but this is a single-operator test harness -- fine for now
    op->env_state = FM_ENV_ATTACK;
    op->fb_history[0] = op->fb_history[1] = 0; // avoid a stale feedback pop carrying over from a previous note
}

void FMOperator_KeyOff(FMOperator *op) {
    op->keyed = 0;
    if (op->env_state != FM_ENV_OFF)
        op->env_state = FM_ENV_RELEASE;
}

// Shared by FMOperator_Clock (lone-operator/self-feedback-only case) and
// FMOperator_ClockMod (multi-operator case, see fm_voice.h) -- ext_phase_mod
// is an already-phase-domain-scaled modulation input from another
// operator's previous output, added on top of this operator's own
// self-feedback. "Previous output" (not this-tick's) is deliberate: it's
// exactly the same one-tick-delay trick FB already uses on itself
// (fb_history), which sidesteps needing to topologically order operators at
// all -- works identically whether the routing graph is a straight chain or
// has cycles (feedback loops through other operators), matching how a
// free-form patch surface needs to behave.
static int16_t ClockInternal(FMOperator *op, int32_t ext_phase_mod, uint32_t am_atten) {
    EnvelopeStep(op);

    op->phase += op->phase_step;

    // Self-feedback: real hardware only ever applies this to operator 1's OWN previous output feeding
    // its own phase, averaging/summing the last two output samples. Hardware scale (MAME fm.cpp, Nuked-OPN2):
    // the phase moves by (sum >> (10 - FB)) sine-table steps, out of 1024 per cycle. Our phase has 2^22
    // units per cycle (index = phase >> 12), so one table step is << 12. (This used to be << 9, tuned by
    // ear "to make feedback audible" -- 8x weaker than the chip: feedback voices came out dull.)
    int32_t phase_mod = ext_phase_mod;
    if (op->fb > 0) {
        int32_t fb_sum = op->fb_history[0] + op->fb_history[1];
        phase_mod += (fb_sum >> (10 - op->fb)) << 12;
    }

    uint32_t modulated_phase = (uint32_t)((int64_t)op->phase + phase_mod);
    uint32_t phase_index = (modulated_phase >> (32 - SINE_BITS - 10)) & (SINE_SIZE - 1); // top SINE_BITS bits select the table position

    int negative;
    uint16_t sine_atten = SineLookup(phase_index, &negative);

    // Combine log-domain attenuations (sine shape + envelope + TL) with a
    // simple add -- this is exactly why the log-sine/exp approach exists on
    // real hardware, a multiply in linear terms becomes an add here.
    //
    // Scale factors: env_level (0-1023) and TL (0-127) each need to span
    // close to the *entire* EXP_SIZE attenuation range on their own (real
    // hardware: either one alone can silence a channel completely), so
    // env_level*(EXP_SIZE/1024) and tl*(EXP_SIZE/128) both land just under
    // EXP_SIZE-1 at their own max. The previous factors (EXP_SIZE/16/32 and
    // EXP_SIZE/16/16) were arbitrary and wrong in opposite directions:
    // env_level's was 2x too strong -- traced directly to a genuinely
    // silent operator despite env_level sitting at only ~60% of its range
    // (613/1023), i.e. total_atten was clamping to full silence at barely
    // half the intended envelope travel -- while TL's was 2x too weak.
    uint32_t total_atten = sine_atten + (op->env_level * (EXP_SIZE / 1024)) + ((uint32_t)op->tl * (EXP_SIZE / 128));
    if (op->am)
        total_atten += am_atten; // LFO tremolo, per-operator opt-in via the AM-on bit -- see this function's declaration comment
    if (total_atten > EXP_SIZE - 1)
        total_atten = EXP_SIZE - 1;

    int16_t sample = (int16_t)exp_table[total_atten];
    if (negative)
        sample = (int16_t)-sample;

    op->fb_history[1] = op->fb_history[0];
    op->fb_history[0] = sample; // doubles as "this operator's last output sample" for fm_voice.h's routing graph to read
    return sample;
}

int16_t FMOperator_Clock(FMOperator *op, uint32_t am_atten) {
    return ClockInternal(op, 0, am_atten);
}

int16_t FMOperator_ClockMod(FMOperator *op, int32_t ext_phase_mod, uint32_t am_atten) {
    return ClockInternal(op, ext_phase_mod, am_atten);
}
