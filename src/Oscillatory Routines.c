#include "Oscillatory Routines.h"

extern Oscillatory oscillatory;

extern Object *const player;

/**
 * OscillateNumInit
 * FIXED VERSION:
 * 1. Mask updated to 0x7C to include Index 9 (bit 6).
 * 2. Uses direct struct assignment for snappiness.
 * 3. Corrects the "Initial Pop" by ensuring the mask matches the starting 0x80 values.
 */
void OscillateNumInit() {
    static const Oscillatory baselines = {
        .direction = 0x007C, // Bits 6, 5, 4, 3, 2 (Indices 9-13)
        .state = {
            {0x80, 0}, {0x80, 0}, {0x80, 0}, {0x80, 0},
            {0x80, 0}, {0x80, 0}, {0x80, 0}, {0x80, 0},
            {0x80, 0},
            {0x50F0, 0x11E}, // Index 9  (Bit 6) - Bkg
            {0x2080, 0xB4},  // Index 10 (Bit 5) - Bkg
            {0x3080, 0x10E}, // Index 11 (Bit 4) - Bkg
            {0x5080, 0x1C2}, // Index 12 (Bit 3) - Bkg
            {0x7080, 0x276}, // Index 13 (Bit 2) - Bkg
            {0x80, 0}, {0x80, 0}
        }
    };
    // Clean struct assignment (faster than memcpy for 66 bytes)
    oscillatory = baselines;
}

/**
 * Optimized OscillateNumDo
 * Now using your 16-bit frequency/amplitude struct!
 */
void OscillateNumDo() {
    if (player->routine >= 6) return;

    // Global speed control (matches user's preferred rate)
    static int8_t update_timer = 0;
    if (--update_timer >= 0) return;
    update_timer = 4;

    // 1. Settings Table
    // Index 4 (Magma): Amp 176 (0xB0) provides a 48-unit swing from baseline 128 (0x80)
    // 48 units / 16 per cycle = exactly 3 cycles.
    static const OscillateSettings settings[16] = {
        {2, 0x10}, {2, 0x18}, {2, 0x20}, {2, 0x30},
        {4, 352},  {8, 0x08}, {8, 0x40}, {4, 0x40}, // Index 4: Amplitude 176
        {2, 0x50}, {2, 0x50}, {2, 0x20}, {3, 0x30},
        {5, 0x50}, {7, 0x70}, {2, 0x10}, {2, 0x10}
    };

    static int8_t pause_timers[16] = {0};
    uint16_t mask = 0x8000;

    for (int i = 0; i < 16; i++, mask >>= 1) {
        // If this oscillator is in a "pause" state, wait it out
        if (pause_timers[i] > 0) {
            pause_timers[i]--;
            continue;
        }

        uint16_t freq = settings[i].frequency;
        uint16_t amp  = settings[i].amplitude;

        // Treat state as signed 16-bit to allow clean arithmetic
        int16_t pos = (int16_t)oscillatory.state[i][0];
        int16_t vel = (int16_t)oscillatory.state[i][1];

        if (!(oscillatory.direction & mask)) {
            // PHASE 1: Move from 128 (Baseline) towards Amplitude Peak
            vel += freq;
            pos += vel;

            // Check if we hit or passed the peak
            if (pos >= amp) {
                oscillatory.state[i][0] = amp;   // Snap to peak (Shift 0)
                oscillatory.state[i][1] = 0;     // Reset velocity
                oscillatory.direction |= mask;   // Reverse direction
                pause_timers[i] = 15;            // Pause for ~2 seconds at 60fps
                continue;
            }
        } else {
            // PHASE 2: Move from Peak back to 128 (Baseline)
            vel -= freq;
            pos += vel;

            // Check if we returned to baseline (128)
            // Use 8-bit check or simple range check
            if (pos <= 128 && pos > 0) {
                oscillatory.state[i][0] = 128;   // Snap to baseline (Shift 0)
                oscillatory.state[i][1] = 0;     // Reset velocity
                oscillatory.direction &= ~mask;  // Reverse direction
                pause_timers[i] = 15;            // Pause for ~2 seconds
                continue;
            }
        }

        // Write back to struct
        oscillatory.state[i][0] = (uint16_t)pos;
        oscillatory.state[i][1] = (uint16_t)vel;
    }
}
