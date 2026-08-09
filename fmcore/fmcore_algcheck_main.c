// Disposable verification jig (not meant to be kept around, see
// fm_voice.h's own framing comment): tests the actual claim behind building
// fm_voice.h as a free-form graph instead of 8 hardcoded special cases --
// that if the general primitives (phase modulation summing, one-tick-delay
// routing, carrier mixing) are correct, ALL 8 of the real YM2612's fixed
// algorithms should just fall out of it as specific connect[][] presets,
// with no algorithm-specific code needed. Builds each of the 8 real
// algorithms both ways -- as a real ymfm chip (ground truth, register-level
// algorithm field) and as an FMVoice connect[][] graph (hand-derived from
// ymfm_fm.ipp's own s_algorithm_ops table, see this file's own comments) --
// and cross-correlates them.
//
// Same "not bit-exact yet" caveat as fmcore_verify_main.c: DT/RS/envelope
// timing are approximations, not real hardware's exact tables, so this
// isn't expecting 1.0 correlation -- it's checking that the SHAPE each
// algorithm produces (serial chain vs parallel carriers vs mixed) is
// structurally sane and in the right ballpark for all 8, not just the ones
// tested so far (chain, and algorithm-7-style all-parallel).

#include "fm_voice.h"

#include "../contrib/ymfm/src/ymfm_opn.h"

#include <math.h>
#include <stdio.h>
#include <vector>

struct Interface : ymfm::ymfm_interface {};

static void RunOurs(int alg, int duration_samples, std::vector<double> &out) {
    FMVoice voice;
    FMVoice_Init(&voice);
    for (int i = 0; i < FM_VOICE_OP_COUNT; i++)
        for (int j = 0; j <= FM_VOICE_OP_COUNT; j++)
            voice.connect[i][j] = FM_VOICE_ALGORITHM_CONNECT[alg][i][j];
    for (int i = 0; i < FM_VOICE_OP_COUNT; i++)
        FMOperator_SetParams(&voice.ops[i], /*dt*/ 0, /*mul*/ 1, /*rs*/ 0, /*ar*/ 31, /*am*/ 0, /*d1r*/ 5,
                              /*d2r*/ 2, /*d1l*/ 4, /*rr*/ 7, /*tl*/ 0, /*fb*/ 0);
    FMVoice_SetFreq(&voice, /*fnum*/ 0x2A4, /*block*/ 4);
    FMVoice_KeyOn(&voice);
    out.resize(duration_samples);
    for (int i = 0; i < duration_samples; i++)
        out[i] = FMVoice_Clock(&voice, 0.0f); // no AM exercised in this check (all ops am=0 above)
}

int main(int argc, char **argv) {
    int duration_samples = (argc > 1) ? atoi(argv[1]) : 4410;

    // Algorithms 0-7: real hardware algorithms, cross-checked against ymfm
    // (same modest params on all 4 operators -- deliberately identical
    // regardless of each operator's role in this algorithm, so any
    // difference in result is purely down to the routing, not per-op
    // tuning choices).
    for (int alg = 0; alg < 8; alg++) {
        std::vector<double> ours;
        RunOurs(alg, duration_samples, ours);

        Interface intf;
        ymfm::ym2612 chip(intf);
        chip.reset();
        auto write = [&](uint32_t offset, uint8_t data) { chip.write(offset, data); };

        write(0, 0xB0);
        write(1, (uint8_t)alg); // channel 1, this algorithm, feedback 0
        write(0, 0xB4);
        write(1, 0xC0); // pan both L+R
        for (int op = 0; op < 4; op++) {
            static const uint8_t base[4] = {0x30, 0x34, 0x38, 0x3C};
            write(0, base[op]);
            write(1, 0x01); // DT/MUL
            write(0, (uint8_t)(base[op] + 0x20));
            write(1, 0x1F); // RS/AR
            write(0, (uint8_t)(base[op] + 0x30));
            write(1, 0x05); // AM/D1R
            write(0, (uint8_t)(base[op] + 0x40));
            write(1, 0x02); // D2R
            write(0, (uint8_t)(base[op] + 0x50));
            write(1, 0x47); // D1L/RR
            write(0, (uint8_t)(base[op] + 0x10));
            write(1, 0x00); // TL
        }
        write(0, 0xA4);
        write(1, 0x24); // block/fnum high
        write(0, 0xA0);
        write(1, 0xA4); // fnum low
        write(0, 0x28);
        write(1, 0xF0); // key on channel 1, all 4 slots

        std::vector<double> theirs(duration_samples);
        ymfm::ym2612::output_data sample{};
        for (int i = 0; i < duration_samples; i++) {
            chip.generate(&sample, 1);
            theirs[i] = sample.data[0];
        }

        double dot = 0, mag_a = 0, mag_b = 0, peak_a = 0, peak_b = 0;
        for (int i = 0; i < duration_samples; i++) {
            dot += ours[i] * theirs[i];
            mag_a += ours[i] * ours[i];
            mag_b += theirs[i] * theirs[i];
            if (fabs(ours[i]) > peak_a)
                peak_a = fabs(ours[i]);
            if (fabs(theirs[i]) > peak_b)
                peak_b = fabs(theirs[i]);
        }
        double correlation = (mag_a > 0 && mag_b > 0) ? dot / sqrt(mag_a * mag_b) : 0;
        printf("Algorithm %2d: our_peak=%6.0f  ymfm_peak=%6.0f  correlation=%+.4f%s\n", alg, peak_a, peak_b,
               correlation, (mag_a == 0) ? "  (OURS SILENT)" : (mag_b == 0 ? "  (YMFM SILENT)" : ""));
    }

    // Algorithms 8-15: fmcore-original, no real hardware equivalent to
    // cross-check against -- just a structural sanity pass (nonzero output,
    // no runaway/NaN-style blowup from the cyclic/reciprocal routings).
    for (int alg = 8; alg < FM_VOICE_ALGORITHM_COUNT; alg++) {
        std::vector<double> ours;
        RunOurs(alg, duration_samples, ours);
        double peak = 0, energy = 0;
        for (double s : ours) {
            if (fabs(s) > peak)
                peak = fabs(s);
            energy += s * s;
        }
        printf("Algorithm %2d: our_peak=%6.0f  energy=%.0f%s (fmcore-original, no ymfm equivalent)\n", alg, peak,
               energy, peak > 32767.0 ? "  *** OUT OF RANGE ***" : (peak == 0 ? "  *** SILENT ***" : ""));
    }
    return 0;
}
