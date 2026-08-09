// Verification harness for fm_operator.c: drives our single operator and
// ymfm's real ym2612 with identical parameters, and compares output.
//
// Isolating a single operator's output from ymfm needs a trick, since ymfm
// only exposes a whole chip's final (post-algorithm) output, never an
// individual operator directly: force algorithm 7 (all 4 operators are
// independent parallel carriers, see Sound.c's FM_SLOT_MASK -- alg 7 mask
// is 0xF, all four bits set) and silence operators 2-4 (TL=127, maximum
// attenuation) so only operator 1's own output reaches the chip's final
// sample. Feedback is left at 0 (no self-modulation) since that's still a
// later step in this side project, same as algorithm routing itself.
//
// This does NOT expect bit-exact agreement yet -- see fm_operator.c's own
// comments on what's still an approximation (envelope rate timing,
// DT/RS). It reports basic shape/correlation stats so there's an actual
// number to track as those pieces get filled in, instead of "sounds about
// right".

#include "fm_operator.h"

#include "../contrib/ymfm/src/ymfm_opn.h"

#include <math.h>
#include <stdio.h>
#include <vector>

struct Interface : ymfm::ymfm_interface {};

int main(int argc, char **argv) {
    int duration_samples = (argc > 1) ? atoi(argv[1]) : 4410; // 0.1s at 44.1kHz

    // --- Our operator ---
    FMOperator op;
    FMOperator_Init(&op);
    FMOperator_SetParams(&op, /*dt*/ 0, /*mul*/ 1, /*rs*/ 0, /*ar*/ 31, /*am*/ 0, /*d1r*/ 5, /*d2r*/ 2, /*d1l*/ 4,
                          /*rr*/ 7, /*tl*/ 0, /*fb*/ 0);
    FMOperator_SetFreq(&op, /*fnum*/ 0x2A4, /*block*/ 4); // ~A4, matches Sound.c's FM_FNUM_TABLE[9] at a mid block
    FMOperator_KeyOn(&op);

    // --- ymfm reference, algorithm 7, only op1 audible ---
    Interface intf;
    ymfm::ym2612 chip(intf);
    chip.reset();
    auto write = [&](uint32_t offset, uint8_t data) { chip.write(offset, data); };

    write(0, 0xB0); write(1, 0x07); // channel 1, algorithm 7 feedback 0
    write(0, 0xB4); write(1, 0xC0); // pan both L+R
    // op1 (slot 0): DT=0 MUL=1 RS=0 AR=31 AM=0 D1R=5 D2R=2 D1L=4 RR=7 TL=0
    write(0, 0x30); write(1, 0x01); // DT/MUL
    write(0, 0x50); write(1, 0x1F); // RS/AR
    write(0, 0x60); write(1, 0x05); // AM/D1R
    write(0, 0x70); write(1, 0x02); // D2R
    write(0, 0x80); write(1, 0x47); // D1L/RR
    write(0, 0x40); write(1, 0x00); // TL
    // op2/op3/op4 (slots 1/2/3): silence via max TL
    write(0, 0x44); write(1, 0x7F);
    write(0, 0x48); write(1, 0x7F);
    write(0, 0x4C); write(1, 0x7F);
    // Frequency, channel 1
    write(0, 0xA4); write(1, 0x24); // block/fnum high
    write(0, 0xA0); write(1, 0xA4); // fnum low
    // Key on channel 1, all 4 slots (silenced ones don't matter)
    write(0, 0x28); write(1, 0xF0);

    std::vector<double> ours(duration_samples), theirs(duration_samples);
    ymfm::ym2612::output_data sample{};
    for (int i = 0; i < duration_samples; i++) {
        ours[i] = FMOperator_Clock(&op, 0); // no AM exercised in this check
        chip.generate(&sample, 1);
        theirs[i] = sample.data[0];
    }

    // Basic shape comparison: normalized cross-correlation at zero lag, plus
    // peak amplitude ratio -- not a claim of correctness, just a number to
    // watch improve as DT/RS/rate-accurate envelope timing get filled in.
    double dot = 0, mag_a = 0, mag_b = 0, peak_a = 0, peak_b = 0;
    for (int i = 0; i < duration_samples; i++) {
        dot += ours[i] * theirs[i];
        mag_a += ours[i] * ours[i];
        mag_b += theirs[i] * theirs[i];
        if (fabs(ours[i]) > peak_a) peak_a = fabs(ours[i]);
        if (fabs(theirs[i]) > peak_b) peak_b = fabs(theirs[i]);
    }
    double correlation = (mag_a > 0 && mag_b > 0) ? dot / sqrt(mag_a * mag_b) : 0;

    printf("Samples: %d\n", duration_samples);
    printf("Our peak amplitude:  %.1f\n", peak_a);
    printf("ymfm peak amplitude: %.1f\n", peak_b);
    printf("Cross-correlation (zero lag): %.4f  (1.0 = identical shape, 0 = unrelated, -1 = inverted)\n", correlation);
    return 0;
}
