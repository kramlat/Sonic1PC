#include "fm_voice.h"

// 0-7: ground truth is contrib/ymfm/src/ymfm_fm.ipp's s_algorithm_ops table
// (see that file's own comment for the opout[] index meanings this was read
// from). Operator indices are 0-based (real hardware's O1-O4 are ops[0]-
// ops[3] here).
//
// 8-15: NOT real hardware algorithms -- fmcore-original routings that
// specifically use what a free-form, one-tick-delay graph can do that real
// YM2612 hardware structurally can't (cyclic modulation loops, reciprocal
// modulator pairs, 3-way fan-in, a modulator that's also its own carrier,
// dense cross-modulation) -- see FMOperator_ClockMod's own comment on why
// the one-tick delay makes cycles safe here at all. Real hardware's
// feedback register only ever self-modulates operator 1 and never forms
// loops THROUGH other operators the way several of these do.
const bool FM_VOICE_ALGORITHM_CONNECT[16][FM_VOICE_OP_COUNT][FM_VOICE_OP_COUNT + 1] = {
    // 0: O1->O2->O3->O4->OUT
    {{0, 1, 0, 0, 0}, {0, 0, 1, 0, 0}, {0, 0, 0, 1, 0}, {0, 0, 0, 0, 1}},
    // 1: (O1+O2)->O3->O4->OUT
    {{0, 0, 1, 0, 0}, {0, 0, 1, 0, 0}, {0, 0, 0, 1, 0}, {0, 0, 0, 0, 1}},
    // 2: (O1+(O2->O3))->O4->OUT
    {{0, 0, 0, 1, 0}, {0, 0, 1, 0, 0}, {0, 0, 0, 1, 0}, {0, 0, 0, 0, 1}},
    // 3: ((O1->O2)+O3)->O4->OUT
    {{0, 1, 0, 0, 0}, {0, 0, 0, 1, 0}, {0, 0, 0, 1, 0}, {0, 0, 0, 0, 1}},
    // 4: ((O1->O2)+(O3->O4))->OUT
    {{0, 1, 0, 0, 0}, {0, 0, 0, 0, 1}, {0, 0, 0, 1, 0}, {0, 0, 0, 0, 1}},
    // 5: (O1->O2)+(O1->O3)+(O1->O4)->OUT
    {{0, 1, 1, 1, 0}, {0, 0, 0, 0, 1}, {0, 0, 0, 0, 1}, {0, 0, 0, 0, 1}},
    // 6: (O1->O2)+O3+O4->OUT
    {{0, 1, 0, 0, 0}, {0, 0, 0, 0, 1}, {0, 0, 0, 0, 1}, {0, 0, 0, 0, 1}},
    // 7: O1+O2+O3+O4->OUT
    {{0, 0, 0, 0, 1}, {0, 0, 0, 0, 1}, {0, 0, 0, 0, 1}, {0, 0, 0, 0, 1}},

    // 8 "Ring": O1->O2->O3->O4->O1 (cyclic), O4->OUT -- an evolving,
    // slowly-building texture as the loop's modulation compounds tick over
    // tick, since each operator's own envelope keeps changing what it
    // feeds back into the loop.
    {{0, 1, 0, 0, 0}, {0, 0, 1, 0, 0}, {0, 0, 0, 1, 0}, {1, 0, 0, 0, 1}},
    // 9 "Reciprocal Pairs": (O1<->O2) + (O3<->O4), O2+O4->OUT -- each pair
    // modulates itself, a denser/buzzier sideband spread than one-way FM.
    {{0, 1, 0, 0, 0}, {1, 0, 0, 0, 1}, {0, 0, 0, 1, 0}, {0, 0, 1, 0, 1}},
    // 10 "Triple Modulator": (O1+O2+O3)->O4->OUT -- three simultaneous
    // modulators summed into one carrier, denser than any real algorithm's
    // 2-modulator max (alg1).
    {{0, 0, 0, 1, 0}, {0, 0, 0, 1, 0}, {0, 0, 0, 1, 0}, {0, 0, 0, 0, 1}},
    // 11 "Diamond": O1 fans to O2 and O3, both converge on O4->OUT -- no
    // real algorithm routes one modulator's output through two independent
    // second-stage modulators before recombining.
    {{0, 1, 1, 0, 0}, {0, 0, 0, 1, 0}, {0, 0, 0, 1, 0}, {0, 0, 0, 0, 1}},
    // 12 "Reciprocal Feeds Carrier": (O1<->O2) reciprocal, both feed O3
    // (carrier); O4 independent carrier.
    {{0, 1, 1, 0, 0}, {1, 0, 1, 0, 0}, {0, 0, 0, 0, 1}, {0, 0, 0, 0, 1}},
    // 13 "Drone Plus Harmonics": O1 fans to O2/O3/O4 like real alg5, but O1
    // is ALSO its own carrier (real alg5's O1 is silent, pure modulator) --
    // a fundamental tone plus three FM-colored harmonics stacked on top.
    {{0, 1, 1, 1, 1}, {0, 0, 0, 0, 1}, {0, 0, 0, 0, 1}, {0, 0, 0, 0, 1}},
    // 14 "Cross Carriers": (O1<->O3) + (O2<->O4) reciprocal pairs, all 4
    // operators also carriers -- every voice audible AND cross-modulating.
    {{0, 0, 1, 0, 1}, {0, 0, 0, 1, 1}, {1, 0, 0, 0, 1}, {0, 1, 0, 0, 1}},
    // 15 "Chaos": every operator modulates every other operator, all 4
    // carry -- maximal cross-modulation density, only tractable at all
    // because of the one-tick-delay trick.
    {{0, 1, 1, 1, 1}, {1, 0, 1, 1, 1}, {1, 1, 0, 1, 1}, {1, 1, 1, 0, 1}},
};

// Max attenuation (EXP_SIZE=4096 log-domain units, same domain as
// fm_operator.c's total_atten) at the LFO's peak, per AMS setting 0-3.
// Real hardware's approximate AMS depths are 0dB/1.4dB/5.9dB/11.8dB; EXP_SIZE
// units are EXP_SIZE/16 = 256 per octave (6.02dB) -- see fm_operator.c's
// BuildTables comment for that scale factor's own derivation -- so
// dB * (256/6.02) converts each depth into this table's units.
static const uint32_t AMS_MAX_ATTEN[4] = {0, 60, 251, 502};

void FMVoice_Init(FMVoice *voice) {
    for (int i = 0; i < FM_VOICE_OP_COUNT; i++) {
        FMOperator_Init(&voice->ops[i]);
        for (int j = 0; j <= FM_VOICE_OP_COUNT; j++)
            voice->connect[i][j] = false;
    }
    voice->ams = 0;
}

void FMVoice_SetAMS(FMVoice *voice, uint8_t ams) { voice->ams = ams & 3; }

void FMVoice_SetFreq(FMVoice *voice, uint16_t fnum, uint8_t block) {
    for (int i = 0; i < FM_VOICE_OP_COUNT; i++)
        FMOperator_SetFreq(&voice->ops[i], fnum, block);
}

void FMVoice_KeyOn(FMVoice *voice) {
    for (int i = 0; i < FM_VOICE_OP_COUNT; i++)
        FMOperator_KeyOn(&voice->ops[i]);
}

void FMVoice_KeyOff(FMVoice *voice) {
    for (int i = 0; i < FM_VOICE_OP_COUNT; i++)
        FMOperator_KeyOff(&voice->ops[i]);
}

int16_t FMVoice_Clock(FMVoice *voice, float lfo_am_unipolar) {
    // Snapshot every operator's PREVIOUS output before any of them tick
    // this sample -- see FMOperator_ClockMod's comment on why this
    // one-tick delay is what makes an arbitrary (possibly cyclic) graph
    // safe to evaluate without a topological sort: every operator this
    // tick reads a fixed, already-settled value regardless of processing
    // order or of the graph looping back on itself through other operators.
    int32_t prev_sample[FM_VOICE_OP_COUNT];
    for (int i = 0; i < FM_VOICE_OP_COUNT; i++)
        prev_sample[i] = voice->ops[i].fb_history[0];

    // This channel's own AMS depth applied to the chip-wide LFO's current
    // value -- log-domain (added straight into total_atten inside
    // ClockInternal), not a post-hoc linear multiply, so it combines with
    // envelope/TL exactly the way real hardware's own log-sine/exp design
    // does. Per-operator AM-on (op->am) is checked inside ClockInternal
    // itself, not here -- every operator gets the same am_atten value and
    // individually decides whether to use it.
    uint32_t am_atten = (uint32_t)(lfo_am_unipolar * (float)AMS_MAX_ATTEN[voice->ams] + 0.5f);

    int32_t mix = 0;
    for (int to = 0; to < FM_VOICE_OP_COUNT; to++) {
        int32_t ext_phase_mod = 0;
        for (int from = 0; from < FM_VOICE_OP_COUNT; from++) {
            if (voice->connect[from][to])
                ext_phase_mod += prev_sample[from] << 9; // same scale FMOperator's own self-feedback uses, see fm_operator.c
        }
        int16_t sample = FMOperator_ClockMod(&voice->ops[to], ext_phase_mod, am_atten);
        if (voice->connect[to][FM_VOICE_OUT])
            mix += sample;
    }

    // Headroom: NOT a flat divide-by-carrier-count (tried that -- see git
    // history -- it made real, legitimate multi-carrier voices like SndB5's
    // ring chime (Algorithm 4, 2 carriers, TL=35 each, nowhere near full
    // scale on their own) noticeably quieter for no reason, since dividing
    // applies regardless of whether the sum was ever going to clip at all).
    // Just clamp -- most real voices (TL attenuation on each carrier) never
    // approach this ceiling; the one case that can (Algorithm 7 with all 4
    // carriers at TL=0) just hits the same hard ceiling real hardware's own
    // DAC would, rather than getting artificially quieter across the board.
    if (mix > 32767)
        mix = 32767;
    if (mix < -32768)
        mix = -32768;
    return (int16_t)mix;
}
