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

// The 16 algorithms: 0-7 are the real YM2612's fixed hardware routings and can never change; 8-15 (8-F) are fmcore-original routings with no hardware
// equivalent (see fm_voice.c's comment on each). The ones in 8-15 are only DEFAULTS: a game may replace any of them with FMVoice_SetCustomAlgorithm
// (the sound driver does it from the game's SoundBank). Everything that needs a routing asks for the one in force through the functions below, so an
// override reaches the chip, the carrier masks (which operators the channel volume attenuates) and the editors alike.
#define FM_VOICE_ALGORITHM_COUNT 16
#define FM_VOICE_HARDWARE_ALGORITHMS 8

typedef bool FMAlgorithmRoute[FM_VOICE_OP_COUNT][FM_VOICE_OP_COUNT + 1]; // [from][to], to == FM_VOICE_OUT: `from` is audible

// The routings as shipped (0-7 hardware, 8-15 fmcore's defaults): reference only, an override does not change them.
extern const FMAlgorithmRoute FM_VOICE_ALGORITHM_DEFAULT[FM_VOICE_ALGORITHM_COUNT];

// Does `from` feed `to` in the routing now in force for `algorithm` (0-15; out of range reads as 0-15 by its low 4 bits)?
bool FMVoice_AlgorithmRoutes(int algorithm, int from, int to);
// Is operator `op` (natural order, 0 = operator 1) audible on its own in that algorithm: the ones the channel volume attenuates.
bool FMVoice_AlgorithmCarrier(int algorithm, int op);
// Copies the routing in force into a voice.
void FMVoice_LoadAlgorithm(FMVoice *voice, int algorithm);
// Replaces algorithm 8-15's routing. Returns false (and changes nothing) for a hardware algorithm or an out-of-range one. Do it between sounds, not
// while the audio thread is playing a voice that uses it.
bool FMVoice_SetCustomAlgorithm(int algorithm, const FMAlgorithmRoute route);
// Puts one 8-15 algorithm, or all of them, back to the shipped default.
void FMVoice_ResetCustomAlgorithm(int algorithm);
void FMVoice_ResetCustomAlgorithms(void);

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
