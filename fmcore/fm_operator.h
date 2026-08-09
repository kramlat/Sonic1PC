#pragma once

// A from-scratch YM2612 FM operator core -- step 1 of a planned "build our
// own OPN2 core" side project (see the conversation this came out of):
// single operator first, no algorithm/feedback/multi-operator combination
// yet. Once this is verified against ymfm (see fmcore_verify_main.c) for a
// single isolated carrier, the plan is an array of 4 of these plus an
// algorithm/feedback struct wrapping them into a real channel.
//
// Deliberately structured the way the real chip actually works (log-domain
// sine + envelope, converted back to linear via an exp table) rather than a
// simpler floating-point sin()*envelope approach, since that log-domain
// quantization is a real, audible part of the YM2612's character -- but the
// envelope generator's exact per-rate timing curve is NOT yet verified
// against real hardware/ymfm's own rate tables (see the comment on
// FMOperator_Clock). Treat this as a structurally-correct skeleton, not a
// bit-exact implementation yet.
//
// DT (detune) and RS (rate scaling) are both applied as approximations, not
// real hardware's exact keycode-indexed ROM tables -- see FMOperator_SetFreq
// and EnvelopeStep's own comments.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FM_ENV_ATTACK,
    FM_ENV_DECAY1,
    FM_ENV_DECAY2,
    FM_ENV_RELEASE,
    FM_ENV_OFF,
} FMEnvState;

typedef struct {
    // Phase accumulator. 20-bit fixed point (matches the real chip's phase
    // counter precision) -- top 10 bits select the sine table position each
    // clock.
    uint32_t phase;
    uint32_t phase_step;

    // Envelope generator. level is a 10-bit attenuation value (0 = loudest,
    // 1023 = silent), same convention the real chip's EG uses internally
    // (this is NOT the same thing as the register TL field, which gets
    // added on top of whatever the EG produces).
    FMEnvState env_state;
    uint32_t env_level;    // 10-bit view, what FMOperator_Clock actually reads
    uint32_t env_level_q8; // Q8 fixed-point accumulator backing env_level for decay/release (attack is multiplicative, doesn't need one) -- lets slow rates step less than 1 whole unit/tick instead of rounding to a no-op

    // Raw register-style parameters, same fields/ranges as a real YM2612
    // operator (DT 0-7, MUL 0-15, RS 0-3, AR/D1R/D2R 0-31, D1L/RR 0-15,
    // TL 0-127, AM 0-1, FB 0-7).
    uint8_t dt, mul, rs, ar, am, d1r, d2r, d1l, rr, tl, fb;

    // Feedback history -- real hardware only ever applies feedback to an
    // operator's OWN previous output feeding back into its OWN phase input
    // (never any other operator's), which is exactly the "lone carrier"
    // case this single-operator stage can test before algorithm routing
    // exists at all. Two samples of history, matching real hardware
    // averaging the previous two outputs before applying the FB shift.
    int32_t fb_history[2];

    // Current note, for rate-scaling (RS combines with block/fnum on real
    // hardware -- not yet implemented, see FMOperator_Clock's comment).
    uint16_t fnum;
    uint8_t block;
} FMOperator;

void FMOperator_Init(FMOperator *op);
void FMOperator_SetParams(FMOperator *op, uint8_t dt, uint8_t mul, uint8_t rs, uint8_t ar, uint8_t am, uint8_t d1r,
                           uint8_t d2r, uint8_t d1l, uint8_t rr, uint8_t tl, uint8_t fb);
void FMOperator_SetFreq(FMOperator *op, uint16_t fnum, uint8_t block);
void FMOperator_KeyOn(FMOperator *op);
void FMOperator_KeyOff(FMOperator *op);

// am_atten: this tick's LFO-driven amplitude-modulation attenuation, in the
// same log-domain EXP_SIZE units as env_level/TL (see fm_operator.c's
// ClockInternal), already scaled by the CHANNEL's AMS depth -- pass 0 if
// the caller has no LFO (e.g. the verification jigs). Only actually applied
// if this operator's own op->am is set, matching real hardware's per-
// operator AM-on bit -- the caller doesn't need to check that itself.
// Computing the instantaneous LFO value is fm_voice.h's job (chip-wide LFO,
// shared across every operator in every channel), not this single-operator
// layer's.

// Advances by one internal FM sample tick (clock/144, matching ymfm's/real
// hardware's actual output rate -- see Sound.c's SOUND_FM_CLOCK comment
// from the rest of this project) and returns that tick's signed output
// sample, already through the log-sine/exp stage, envelope/TL/AM
// attenuation, and self-feedback (fb_history, if fb > 0). No modulation
// input FROM another operator -- see FMOperator_ClockMod for that, and
// fm_voice.h for the multi-operator routing graph built on top of it.
int16_t FMOperator_Clock(FMOperator *op, uint32_t am_atten);

// Same as FMOperator_Clock, but also adds ext_phase_mod (an already
// phase-domain-scaled modulation input, typically another operator's own
// last output sample -- see fm_voice.h) on top of this operator's own
// self-feedback before the sine lookup. This is the multi-operator building
// block: fm_voice.h's free-form patch graph calls this once per operator
// per tick with the sum of whatever other operators are wired into it.
int16_t FMOperator_ClockMod(FMOperator *op, int32_t ext_phase_mod, uint32_t am_atten);

#ifdef __cplusplus
}
#endif
