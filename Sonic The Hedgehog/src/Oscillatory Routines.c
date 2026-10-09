#include "Oscillatory Routines.h"

extern Object *const player;

void OscillateNumInit(void) {
    Oscillator_Init(&oscillator_data);
}

void OscillateNumDo(void) {
    if (player->routine >= 6)
        return;
    Oscillator_Step(&oscillator_data);
}
