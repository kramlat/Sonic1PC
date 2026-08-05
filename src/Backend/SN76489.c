// SN76489 PSG emulation: 3 tone channels + 1 noise channel, each with a
// 4-bit (2dB/step) attenuation. Matches the documented behavior of the
// chip as used in the Genesis (noise LFSR reset to 0x8000 on every write
// to the noise control register, "sync to tone 2" noise mode, etc).

#include "SN76489.h"

#include <string.h>

static const int16_t volume_table[16] = {
    8191, 6507, 5168, 4105, 3261, 2590, 2057, 1634,
    1298, 1031, 819, 650, 516, 410, 325, 0,
};

void SN76489_Init(SN76489 *chip) {
    memset(chip, 0, sizeof(*chip));

    for (int c = 0; c < 3; c++) {
        chip->tone_atten[c] = 0xF; // Power-on state: fully attenuated (silent)
        chip->tone_counter[c] = 1;
    }
    chip->noise_atten = 0xF;
    chip->noise_lfsr = 0x8000;
    chip->noise_counter = 1;
}

void SN76489_Write(SN76489 *chip, uint8_t value) {
    if (value & 0x80) {
        // LATCH/DATA byte: 1cctdddd -- selects channel cc and register type
        // t (0 = tone/noise-control, 1 = volume), and supplies its low 4
        // data bits in the same byte.
        uint8_t channel = (value >> 5) & 3;
        uint8_t is_volume = (value >> 4) & 1;
        uint8_t data = value & 0xF;

        chip->latched_channel = channel;
        chip->latched_is_volume = is_volume;

        if (is_volume) {
            if (channel == 3)
                chip->noise_atten = data;
            else
                chip->tone_atten[channel] = data;
        } else if (channel == 3) {
            chip->noise_fb_white = (uint8_t)((data >> 2) & 1);
            chip->noise_shift_rate = (uint8_t)(data & 3);
            chip->noise_lfsr = 0x8000; // Real hardware resets the LFSR on every write here
        } else {
            chip->tone_period[channel] = (uint16_t)((chip->tone_period[channel] & 0x3F0) | data);
        }
    } else {
        // DATA-only byte (bit7 clear): 00dddddd, continues whichever
        // register the last LATCH byte selected.
        uint8_t data = value & 0x3F;

        if (chip->latched_is_volume) {
            if (chip->latched_channel == 3)
                chip->noise_atten = (uint8_t)(data & 0xF);
            else
                chip->tone_atten[chip->latched_channel] = (uint8_t)(data & 0xF);
        } else if (chip->latched_channel == 3) {
            chip->noise_fb_white = (uint8_t)((data >> 2) & 1);
            chip->noise_shift_rate = (uint8_t)(data & 3);
            chip->noise_lfsr = 0x8000;
        } else {
            chip->tone_period[chip->latched_channel] =
                (uint16_t)((chip->tone_period[chip->latched_channel] & 0xF) | (data << 4));
        }
    }
}

// Advances every channel's counter/LFSR by one internal tick -- the chip's
// input clock already divided by 16, which is what tone_period and the
// noise shift rates are specified in terms of.
static void Step(SN76489 *chip) {
    for (int c = 0; c < 3; c++) {
        if (--chip->tone_counter[c] <= 0) {
            uint16_t period = chip->tone_period[c];
            chip->tone_counter[c] = (int16_t)(period == 0 ? 1 : period);
            chip->tone_output[c] ^= 1;
        }
    }

    uint16_t noise_period;
    if (chip->noise_shift_rate == 3)
        noise_period = chip->tone_period[2]; // "Sync" mode: share tone channel 2's period
    else
        noise_period = (uint16_t)(0x20 << chip->noise_shift_rate); // N/512, N/1024, N/2048 of the *input* clock

    if (--chip->noise_counter <= 0) {
        chip->noise_counter = (int16_t)(noise_period == 0 ? 1 : noise_period);

        uint16_t feedback = chip->noise_fb_white
                                 ? (uint16_t)((chip->noise_lfsr ^ (chip->noise_lfsr >> 3)) & 1) // White noise: tapped bits 0 and 3
                                 : (uint16_t)(chip->noise_lfsr & 1);                             // Periodic noise: single tap
        chip->noise_lfsr = (uint16_t)((chip->noise_lfsr >> 1) | (feedback << 15));
    }
}

void SN76489_Generate(SN76489 *chip, int32_t *out, uint32_t count, uint32_t sample_rate, uint32_t clock_rate) {
    uint32_t ticks_num = clock_rate;
    uint32_t ticks_den = sample_rate * 16; // The chip's own fixed /16 input divider

    for (uint32_t s = 0; s < count; s++) {
        chip->tick_accum += ticks_num;
        while (chip->tick_accum >= ticks_den) {
            chip->tick_accum -= ticks_den;
            Step(chip);
        }

        int32_t sample = 0;
        for (int c = 0; c < 3; c++)
            sample += chip->tone_output[c] ? volume_table[chip->tone_atten[c]] : -volume_table[chip->tone_atten[c]];
        sample += (chip->noise_lfsr & 1) ? volume_table[chip->noise_atten] : -volume_table[chip->noise_atten];

        out[s] += sample;
    }
}
