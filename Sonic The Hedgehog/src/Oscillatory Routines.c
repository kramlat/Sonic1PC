#include "Oscillatory Routines.h"

extern Oscillatory oscillatory;

extern Object *const player;

void OscillateNumInit(void) {
    oscillatory.direction = oscillator_data.direction;
    for (int i = 0; i < 16; i++) {
        oscillatory.state[i][0] = oscillator_data.start[i][0];
        oscillatory.state[i][1] = oscillator_data.start[i][1];
    }
}


void OscillateNumDo(void) {
    if (player->routine >= 6)
        return;

    for (int i = 0; i < 16; i++) {
        uint16_t frequency = oscillator_data.settings[i].frequency;
        uint16_t amplitude = oscillator_data.settings[i].amplitude;
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
