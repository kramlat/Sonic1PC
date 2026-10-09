#include "Oscillator.h"

Oscillatory oscillatory;

void Oscillator_Init(const OscillatorData *data) {
    oscillatory.direction = data->direction;
    for (int i = 0; i < 16; i++) {
        oscillatory.state[i][0] = data->start[i][0];
        oscillatory.state[i][1] = data->start[i][1];
    }
}

// One frame (the original's OscillateNumDo): going up, the rate gains the oscillator's frequency and the value gains the rate, and it turns down when the amplitude is no longer above the value's high byte
// (cmp.b 0(a1),d4 / bhi, so equal turns it); going down, the rate loses it and the value gains the rate, and it turns up when the amplitude is above
void Oscillator_Step(const OscillatorData *data) {
    for (int i = 0; i < 16; i++) {
        uint16_t frequency = data->settings[i].frequency;
        uint16_t amplitude = data->settings[i].amplitude;
        uint16_t bit = (uint16_t)(1 << (15 - i));

        if (!(oscillatory.direction & bit)) {
            oscillatory.state[i][1] += frequency;
            oscillatory.state[i][0] += oscillatory.state[i][1];
            if (!(amplitude > (uint8_t)(oscillatory.state[i][0] >> 8)))
                oscillatory.direction |= bit;
        } else {
            oscillatory.state[i][1] -= frequency;
            oscillatory.state[i][0] += oscillatory.state[i][1];
            if (amplitude > (uint8_t)(oscillatory.state[i][0] >> 8))
                oscillatory.direction &= (uint16_t)~bit;
        }
    }
}
