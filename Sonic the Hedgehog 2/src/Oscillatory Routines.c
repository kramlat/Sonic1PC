// Sonic 2's oscillators (the Simon Wai prototype's OscillateNumInit / Oscillate_Num_Do): Sonic 1's with the prototype's start values (entries 9 and 15, and entry 15 going down at the start) and its amplitudes for
// entries 8, 9, 14 and 15 ($38, $38, $40, $40; Sonic 1 has $50, $50, $10, $10)
#include "Oscillatory Routines.h"

extern Oscillatory oscillatory;

extern Object *const player;

void OscillateNumInit(void) {
    Oscillatory baselines = {
        .direction = 0x007D,  // %0000000001111101
        .state = {
            {0x80, 0},
            {0x80, 0},
            {0x80, 0},
            {0x80, 0},
            {0x80, 0},
            {0x80, 0},
            {0x80, 0},
            {0x80, 0},
            {0x80, 0},
            {0x3848, 0xEE},
            {0x2080, 0xB4},
            {0x3080, 0x10E},
            {0x5080, 0x1C2},
            {0x7080, 0x276},
            {0x80, 0},
            {0x4000, 0xFE}
        }
    };

    memcpy(&oscillatory, &baselines, sizeof(Oscillatory));
}


void OscillateNumDo(void) {
    if (player->routine >= 6)
        return;

   const OscillateSettings settings[16] = {
        {2, 0x10},
        {2, 0x18},
        {2, 0x20},
        {2, 0x30},
        {4, 0x20},
        {8, 8},
        {8, 0x40},
        {4, 0x40},
        {2, 0x38},
        {2, 0x38},
        {2, 0x20},
        {3, 0x30},
        {5, 0x50},
        {7, 0x70},
        {2, 0x40},
        {2, 0x40}
    };

    for (int i = 0; i < 16; i++) {
        uint16_t frequency = settings[i].frequency;
        uint16_t amplitude = settings[i].amplitude;
        bool is_down = (oscillatory.direction & (1 << (15 - i))) != 0;

        if (!is_down) {
            oscillatory.state[i][1] += frequency;
            oscillatory.state[i][0] += oscillatory.state[i][1];
            // Original: `cmp.b 0(a1),d4` then `bhi` -- it only stays "up" while amplitude > value,
            // so equality already flips to down.
            if (!(amplitude > (uint8_t)(oscillatory.state[i][0] >> 8))) {
                oscillatory.direction |= (1 << (15 - i));
            }
        } else {
            oscillatory.state[i][1] -= frequency;
            oscillatory.state[i][0] += oscillatory.state[i][1];
            // Original: `cmp.b 0(a1),d4` then `bls` -- only flips back to up while amplitude > value.
            if (amplitude > (uint8_t)(oscillatory.state[i][0] >> 8)) {
                oscillatory.direction &= ~(1 << (15 - i));
            }
        }
    }
}
