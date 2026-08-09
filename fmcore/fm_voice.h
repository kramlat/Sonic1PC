#pragma once

// A 4-operator FM voice built on top of fm_operator.h's single-operator
// core, wired together with a free-form patch graph: any operator can
// modulate any OTHER operator's phase, and any operator can independently
// reach the audio output ("carrier"), in any combination -- not limited to
// the 8 fixed routings real YM2612 hardware supports. The real hardware
// algorithms are just 8 specific presets of this exact same graph (a later
// phase will hardcode those 8 as constant `connect[][]` tables reusing this
// same engine unchanged), so building the general case first means that
// phase is filling in data, not writing new engine code.

#include "fm_operator.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FM_VOICE_OP_COUNT 4
#define FM_VOICE_OUT FM_VOICE_OP_COUNT // pseudo-node index meaning "reaches the audio output"

typedef struct {
    FMOperator ops[FM_VOICE_OP_COUNT];
    // connect[from][to]: true if operator `from`'s previous output feeds
    // operator `to`'s phase input this tick. `to` may also be FM_VOICE_OUT,
    // meaning `from` contributes directly to the voice's mixed audio output
    // -- a single operator can both modulate another operator AND be
    // audible itself at the same time, same as some of the real 8
    // algorithms allow.
    bool connect[FM_VOICE_OP_COUNT][FM_VOICE_OP_COUNT + 1];

    // AMS: this channel's LFO amplitude-modulation SENSITIVITY (register
    // $B4/$B5/$B6, bits 5-4, values 0-3 -- 0=off, 3=deepest, ~11.8dB at the
    // LFO's peak). The LFO itself (register $22: chip-wide enable +
    // frequency) is NOT per-channel -- it's computed once per tick by
    // whatever owns all 6 FMVoices (the YM2612 backend) and passed into
    // FMVoice_Clock as `lfo_am_unipolar` below, exactly matching how real
    // hardware has ONE free-running LFO shared by every channel, each of
    // which applies it at its own independent depth.
    uint8_t ams;
} FMVoice;

// connect[from][to] for 16 algorithms: 0-7 are the real YM2612's fixed
// hardware algorithms (hand-derived from ymfm_fm.ipp's own s_algorithm_ops
// table, contrib/ymfm/src/ymfm_fm.ipp, ground truth), 8-15 are fmcore-
// original routings with no real hardware equivalent (see fm_voice.c's own
// comment on each). Shared single source of truth so the GUI's algorithm
// presets and the ymfm cross-check jig (fmcore_algcheck_main.c) can't drift
// out of sync with each other.
#define FM_VOICE_ALGORITHM_COUNT 16
extern const bool FM_VOICE_ALGORITHM_CONNECT[FM_VOICE_ALGORITHM_COUNT][FM_VOICE_OP_COUNT][FM_VOICE_OP_COUNT + 1];

void FMVoice_Init(FMVoice *voice);
void FMVoice_SetFreq(FMVoice *voice, uint16_t fnum, uint8_t block);
void FMVoice_KeyOn(FMVoice *voice);
void FMVoice_KeyOff(FMVoice *voice);
void FMVoice_SetAMS(FMVoice *voice, uint8_t ams); // clamps to 0-3, see FMVoice::ams

// Advances all 4 operators by one tick and returns the mixed, clamped
// output sample. Modulation routing reads each operator's PREVIOUS tick's
// output (see FMOperator_ClockMod's own comment for why) -- this one-tick
// delay is what lets the graph be free-form/cyclic without needing to
// topologically sort it first.
//
// lfo_am_unipolar: the chip-wide LFO's CURRENT amplitude value, 0.0 (LFO
// trough) to 1.0 (LFO peak), already advanced by whoever owns every
// channel's FMVoice this tick -- pass a constant 0.0f if there's no LFO
// (e.g. the verification jigs). This voice scales it by its own AMS depth
// and applies it only to operators with AM turned on (see fm_operator.h's
// FMOperator_Clock/ClockMod).
int16_t FMVoice_Clock(FMVoice *voice, float lfo_am_unipolar);

#ifdef __cplusplus
}
#endif
