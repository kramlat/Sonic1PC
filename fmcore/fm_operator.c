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
// NOT bit-exact against real hardware's own empirically-derived per-rate
// timing table (that's a specific multi-step pattern per rate, not a smooth
// formula) -- but calibrated against a real, concrete constraint instead of
// being an arbitrary guess: a human takes roughly 9ms to perceive a
// discrete event, so even the FASTEST possible rate must take noticeably
// longer than that to be audible as a note at all, not a silent click. An
// earlier version of this code violated that badly (measured: full decay
// in under 1ms for typical rate values -- see the conversation this fix
// came out of), which got RATE_TIME_MIN_MS bumped to 12ms. That overshot
// the other way once FM SFX actually started reaching this code at all
// (LoadSFX previously never loaded a voice, so this envelope timing was
// never actually audible on a real short SFX until that got fixed) --
// max-rate (AR/D1R/D2R=31) blips like SndB5's ring chime read as
// noticeably sluggish/mushy at 12ms instead of the sharp transient real
// hardware produces. Retuned to 4ms: still well clear of the sub-1ms
// "inaudible click" floor that caused the original bump, but snappy enough
// for punchy percussive SFX. Re-tune by ear via ParadoxComposer's voice
// preview if it still feels off in either direction. Rates now span a
// smooth geometric curve from RATE_TIME_MIN_MS (fastest, rate=max) to
// RATE_TIME_MAX_MS (slowest, rate=1), built once into small per-tick-step
// tables (attack is multiplicative -- exponential approach to 0, matching
// the real chip's general shape even if not its exact curve; decay/release
// are additive, tracked in Q8 fixed point so slow rates can step less than
// 1 unit/tick without just rounding to a no-op).
#define OP_UPDATE_RATE      53267u // SOUND_FM_CLOCK/144, see FMOperator_SetFreq's derivation comment
#define RATE_TIME_MIN_MS    4.0    // fastest rate (AR/D1R/D2R=31, RR=15): time to traverse full scale
#define RATE_TIME_MAX_MS    3000.0 // slowest rate (=1): time to traverse full scale
#define ENV_FULL_Q8         (1023u << 8)

static uint32_t attack_mult_q16[32];  // multiplicative decay-toward-0 factor per tick, Q16 (rate 0 unused -- special-cased)
static uint32_t decay_step_q8[32];    // additive step per tick, Q8, for a 0-31 range field (D1R/D2R)
static uint32_t release_step_q8[16];  // additive step per tick, Q8, for a 0-15 range field (RR)
static int env_tables_built = 0;

static void BuildEnvTables(void) {
    if (env_tables_built)
        return;
    env_tables_built = 1;

    for (int rate = 1; rate <= 31; rate++) {
        double time_ms = RATE_TIME_MIN_MS * pow(RATE_TIME_MAX_MS / RATE_TIME_MIN_MS, (31.0 - rate) / 30.0);
        double ticks = time_ms / 1000.0 * OP_UPDATE_RATE;
        // Multiplicative factor f such that f^ticks ~= 1/1024 (decayed below
        // one LSB of the 10-bit envelope range in that many ticks).
        double f = pow(1.0 / 1024.0, 1.0 / ticks);
        attack_mult_q16[rate] = (uint32_t)(f * 65536.0 + 0.5);
        decay_step_q8[rate] = (uint32_t)(ENV_FULL_Q8 / ticks + 0.5);
        if (decay_step_q8[rate] < 1)
            decay_step_q8[rate] = 1;
    }
    for (int rate = 1; rate <= 15; rate++) {
        double time_ms = RATE_TIME_MIN_MS * pow(RATE_TIME_MAX_MS / RATE_TIME_MIN_MS, (15.0 - rate) / 14.0);
        double ticks = time_ms / 1000.0 * OP_UPDATE_RATE;
        release_step_q8[rate] = (uint32_t)(ENV_FULL_Q8 / ticks + 0.5);
        if (release_step_q8[rate] < 1)
            release_step_q8[rate] = 1;
    }
}

// Rate Scaling (RS): real hardware makes higher-pitched notes run their
// envelope rates faster than low ones (physically realistic -- a plucked
// high string decays quicker than a low one), driven by a 5-bit "key code"
// (kc) derived from block + a small note-within-octave lookup from fnum's
// top bits. Approximated here directly from fnum's own top 2 bits instead
// of the exact lookup table, consistent with this project's "musically
// correct, not hardware-exact" scope. Real hardware operates in a 0-63
// internal rate domain (kc can add up to 31); our rate tables are built for
// the register-level 0-31 (AR/D1R/D2R) and 0-15 (RR) domains instead, so
// the kc contribution is halved to fit before adding -- otherwise RS=3
// would slam every rate to max regardless of its base value, drowning out
// the rate you actually set.
static uint8_t KeyCode(const FMOperator *op) {
    return (uint8_t)((op->block << 2) | ((op->fnum >> 9) & 3));
}

static uint8_t ScaledRate(uint8_t base_rate, uint8_t max_rate, uint8_t rs, uint8_t kc) {
    if (rs == 0 || base_rate == 0) // RS off, or a real "0 = special/never" rate -- scaling that would be wrong
        return base_rate;
    uint32_t rks = (uint32_t)kc >> (3 - rs);
    uint32_t scaled = base_rate + rks / 2;
    return (uint8_t)(scaled > max_rate ? max_rate : scaled);
}

static void EnvelopeStep(FMOperator *op) {
    uint8_t kc = KeyCode(op);
    switch (op->env_state) {
    case FM_ENV_ATTACK: {
        if (op->ar == 0) // AR=0 never attacks, matches real hardware
            break;
        uint64_t level_q16 = (uint64_t)op->env_level << 16;
        level_q16 = (level_q16 * attack_mult_q16[ScaledRate(op->ar, 31, op->rs, kc)]) >> 16;
        op->env_level = (uint32_t)(level_q16 >> 16);
        if (op->env_level <= 1) {
            op->env_level = 0;
            op->env_level_q8 = 0; // Attack tracks env_level via its own local (level_q16), never touches this --
                                   // must sync it here or Decay1's first tick adds its step on top of a stale
                                   // (near-max) value and overshoots the D1L target in ~1 tick regardless of
                                   // D1R/RS, which is why Decay1 was measured taking the same ~5.2ms no matter
                                   // what D1R or RS were set to (should have varied from ~90ms to ~400ms+).
            op->env_state = FM_ENV_DECAY1;
        }
        break;
    }
    case FM_ENV_DECAY1: {
        // D1L is a coarse 4-bit level (0-15), scaled into the 10-bit
        // envelope domain the same way real hardware's own SL lookup does:
        // value*32 (0-480), except D1L=15 which real hardware special-cases
        // to jump straight to 992 instead of the linear 15*32=480 -- a
        // documented quirk of the real chip's table, not a typo. (An
        // earlier version of this used *64 uniformly, which doubled every
        // D1L value's attenuation -- traced directly to D1L=8's default
        // already landing around -48dB before Decay2 even started, making
        // D2R's rate invisible since there was nothing left to hear by the
        // time it ran, and making every note sound like a fast percussive
        // hit regardless of sustain settings.)
        uint32_t target = (op->d1l == 15) ? 992u : (uint32_t)op->d1l * 32u;
        if (op->env_level >= target) {
            // Must sync env_level_q8 to match here too (not just the other
            // exit path below) -- this branch is the one actually taken
            // whenever D1L=0, since env_level is already 0 right out of
            // Attack. Without this, env_level_q8 carries over whatever
            // stale value it had (init, or a previous note's release/decay2
            // state), so Decay2's additive step lands on top of a
            // near-maxed accumulator instead of 0 -- the note reads as
            // already fully attenuated the instant Decay2 starts,
            // regardless of D2R. (Found via D1L=0 making D2R appear to do
            // nothing at all.)
            op->env_level = target;
            op->env_level_q8 = target << 8;
            op->env_state = FM_ENV_DECAY2;
            break;
        }
        if (op->d1r == 0) // real hardware: rate 0 here just never reaches the target, stays in this phase forever
            break;
        op->env_level_q8 += decay_step_q8[ScaledRate(op->d1r, 31, op->rs, kc)];
        op->env_level = op->env_level_q8 >> 8;
        if (op->env_level >= target) {
            op->env_level = target;
            op->env_level_q8 = target << 8;
            op->env_state = FM_ENV_DECAY2;
        }
        break;
    }
    case FM_ENV_DECAY2: {
        if (op->d2r == 0) // D2R=0 is a real "hold forever" value, matches real hardware
            break;
        op->env_level_q8 += decay_step_q8[ScaledRate(op->d2r, 31, op->rs, kc)];
        if (op->env_level_q8 > ENV_FULL_Q8)
            op->env_level_q8 = ENV_FULL_Q8;
        op->env_level = op->env_level_q8 >> 8;
        break;
    }
    case FM_ENV_RELEASE: {
        uint8_t base_rr = op->rr > 0 ? op->rr : 1; // RR=0 real-hardware-invalid for release; treat as the slowest real value instead of stalling forever
        uint32_t step = release_step_q8[ScaledRate(base_rr, 15, op->rs, kc)];
        op->env_level_q8 += step;
        if (op->env_level_q8 >= ENV_FULL_Q8) {
            op->env_level_q8 = ENV_FULL_Q8;
            op->env_level = 1023;
            op->env_state = FM_ENV_OFF;
        } else {
            op->env_level = op->env_level_q8 >> 8;
        }
        break;
    }
    case FM_ENV_OFF:
        op->env_level = 1023;
        break;
    }
}

// ---------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------

void FMOperator_Init(FMOperator *op) {
    BuildTables();
    BuildEnvTables();
    op->phase = 0;
    op->phase_step = 0;
    op->env_state = FM_ENV_OFF;
    op->env_level = 1023;
    op->env_level_q8 = 1023u << 8;
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
// Detune (DT): real hardware's own DT table is a small, keycode-dependent
// (block + top fnum bits) offset added to the phase increment, looked up
// per-note from a fixed ROM table -- not a simple percentage bend. Per this
// project's "musically correct, not hardware-exact" scope, approximated
// instead as a symmetric percentage of the note's own base phase_step,
// using the non-monotonic 7-position magnitude/sign sequence confirmed
// earlier (this same side project's session) against GENNY's own detune
// slider, which mirrors the real chip's redundant-zero raw encoding
// (0=none, 1..3=increasingly sharp, 4=also none, 5..7=increasingly flat).
// Step size bumped from an initial 0.4%/step (1.2% max, ~20 cents -- too
// close to the edge of human pitch discrimination to reliably notice,
// especially with no simultaneous reference tone to beat against) to
// 1%/step (3% max), landing inside real hardware's own actual DT
// deviation range instead of well under it.
static const int8_t DT_SIGN[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const uint8_t DT_MAG[8] = {0, 1, 2, 3, 0, 1, 2, 3}; // step count, same magnitude for the +/- mirror
#define DT_STEP_PERMILLE 10 // ~1%/step of the note's own phase_step -- tuned by ear, not derived from hardware docs

// Shared by FMOperator_SetFreq and FMOperator_SetParams -- real hardware
// re-reads DT/MUL/fnum/block live on every operator update, no key-on
// latching involved, so either one changing must recompute phase_step
// immediately (previously only FMOperator_SetFreq did this, meaning a DT
// slider drag was silently inaudible until the next note-on -- exactly the
// kind of thing that makes a genuinely-present small pitch shift feel like
// nothing happened at all).
static void RecomputePhaseStep(FMOperator *op) {
    uint32_t mul_x2 = op->mul ? (uint32_t)op->mul * 2 : 1; // MUL=0 means x0.5
    uint32_t base_step = ((uint32_t)op->fnum << op->block) * mul_x2;
    int32_t detune = DT_SIGN[op->dt] * (int32_t)(base_step * DT_MAG[op->dt] * DT_STEP_PERMILLE / 1000);
    op->phase_step = (uint32_t)((int32_t)base_step + detune);
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
    op->phase = 0; // real hardware does NOT reset phase on key-on for most cases, but this is a single-operator test harness -- fine for now
    op->env_state = FM_ENV_ATTACK;
    op->fb_history[0] = op->fb_history[1] = 0; // avoid a stale feedback pop carrying over from a previous note
}

void FMOperator_KeyOff(FMOperator *op) {
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

    // Self-feedback: real hardware only ever applies this to an operator's
    // OWN previous output feeding into its OWN phase, never any other
    // operator's -- averaging the last two output samples, shifted by
    // (10-FB), matches how real hardware scales it (FB=0 off, FB=7 the
    // deepest/buzziest self-modulation). The extra <<9 scale factor from
    // "output sample amplitude" into "phase units" is NOT derived from
    // hardware docs -- it's tuned so FB's audible effect actually shows up
    // given this project's own phase/output domains, matching the
    // "musically correct, not hardware-exact" approach the whole side
    // project is going for. Expect to re-tune this by ear once there's a
    // real note to listen to via FMCoreGUI. The same <<9 scale is reused
    // for ext_phase_mod (see FMOperator_ClockMod) for consistency -- one
    // tunable "how strong does modulation feel" knob instead of two.
    int32_t phase_mod = ext_phase_mod;
    if (op->fb > 0) {
        int32_t fb_sum = op->fb_history[0] + op->fb_history[1];
        phase_mod += (fb_sum >> (10 - op->fb)) << 9;
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
