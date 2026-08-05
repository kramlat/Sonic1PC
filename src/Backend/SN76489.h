#pragma once

#include <stdint.h>

// Emulates the SN76489 PSG (Programmable Sound Generator): 3 square-wave
// tone channels and 1 noise channel, each with its own 4-bit attenuation.
// One instance is one physical chip -- Sound.h's dual-chip-set design (see
// sound_music/sound_sfx) means two of these get instantiated, one per set,
// and their output gets downmixed in software afterward.
typedef struct {
    // Tone channels 0-2: 10-bit period + countdown counter, current output
    // polarity, and 4-bit attenuation (0 = loudest, 15 = silent).
    uint16_t tone_period[3];
    int16_t tone_counter[3];
    uint8_t tone_output[3]; // 0 or 1
    uint8_t tone_atten[3];

    // Noise channel: shift rate mode (0-3; 3 = sync to tone channel 2),
    // feedback mode (0 = periodic, 1 = white), its own countdown counter
    // (shared period comes from shift_rate/tone_period[2]), the 16-bit LFSR
    // itself, and its attenuation.
    uint8_t noise_shift_rate;
    uint8_t noise_fb_white;
    int16_t noise_counter;
    uint16_t noise_lfsr;
    uint8_t noise_atten;

    // Latched register: which channel/type a lone data byte (high bit
    // clear) continues -- matches the real chip's write protocol.
    uint8_t latched_channel;
    uint8_t latched_is_volume;

    // Fractional-tick accumulator carried between SN76489_Generate() calls,
    // so the chip's clock/sample-rate ratio doesn't need to divide evenly
    // and successive calls don't drift or click at the boundary.
    uint32_t tick_accum;
} SN76489;

void SN76489_Init(SN76489 *chip);

// Matches a real write to the chip's single I/O port.
void SN76489_Write(SN76489 *chip, uint8_t value);

// Renders `count` mono samples (already summed across all 4 channels) at
// the given output sample rate, additively into `out` (does not clear it
// first -- so multiple chips, or a chip plus other sources, can mix
// straight into the same buffer). `clock_rate` is the chip's input clock
// (the Genesis feeds it the same ~3.58MHz/3.58MHz NTSC/PAL clock as the
// YM2612 gets its own divided-down clock from).
void SN76489_Generate(SN76489 *chip, int32_t *out, uint32_t count, uint32_t sample_rate, uint32_t clock_rate);
