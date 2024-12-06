#include "Oscillatory Routines.h"

extern Oscillatory oscillatory;

extern Object *const player;

void OscillateNumInit() {
    Oscillatory baselines = {
        .direction = 0x003C,  // %0000000001111100
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
            {0x50F0, 0x11E},
            {0x2080, 0xB4},
            {0x3080, 0x10E},
            {0x5080, 0x1C2},
            {0x7080, 0x276},
            {0x80, 0},
            {0x80, 0}
        }
    };

    memcpy(&oscillatory, &baselines, sizeof(Oscillatory));
}

void OscillateNumDo() {
    if (player->routine >= 6) {
        return;
    }

    const OscillateSettings settings[16] = {
        {2, 0x10},
        {2, 0x18},
        {2, 0x20},
        {2, 0x30},
        {4, 0x20},
        {8, 8},
        {8, 0x40},
        {4, 0x40},
        {2, 0x50},
        {2, 0x50},
        {2, 0x20},
        {3, 0x30},
        {5, 0x50},
        {7, 0x70},
        {2, 0x10},
        {2, 0x10}
    };

    for (int i = 0; i < 16; i++) {
        uint16_t frequency = settings[i].frequency;
        uint16_t amplitude = settings[i].amplitude;
        bool is_down = (oscillatory.direction & (1 << (15 - i))) != 0;

        if (!is_down) {
            oscillatory.state[i][1] += frequency;
            oscillatory.state[i][0] += oscillatory.state[i][1];
            if ((uint8_t)oscillatory.state[i][0] > amplitude) {
                oscillatory.direction |= (1 << (15 - i));
            }
        } else {
            oscillatory.state[i][1] -= frequency;
            oscillatory.state[i][0] += oscillatory.state[i][1];
            if ((uint8_t)oscillatory.state[i][0] <= amplitude) {
                oscillatory.direction &= ~(1 << (15 - i));
            }
        }
    }
}
