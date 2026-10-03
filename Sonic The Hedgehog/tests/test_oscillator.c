#include "test.h"

#include <string.h>

#include "Level.h"
#include "Object.h"
#include "Oscillatory Routines.h"

// OscillateNumDo compares with `cmp.b 0(a1),d4` + bhi/bls: the direction only
// stays/returns "up" while amplitude > value, so a value byte EQUAL to the
// amplitude flips direction. The port used >/<= and flipped one step late.

static void Oscillator_FlipsDownWhenValueEqualsAmplitude(void) {
    OscillateNumInit();
    memset(player, 0, sizeof(Object));
    player->routine = 2;
    oscillatory.direction = 0; // everything heading up
    // Entry 8 (SLZ circling platforms, amplitude $50): rate $FE+2 -> value
    // $4F00 + $100 = $5000, byte == amplitude.
    oscillatory.state[8][0] = 0x4F00;
    oscillatory.state[8][1] = 0x00FE;
    OscillateNumDo();
    CHECK((oscillatory.direction & (1 << (15 - 8))) != 0);
}

static void Oscillator_FlipsUpWhenValueBelowAmplitude(void) {
    OscillateNumInit();
    memset(player, 0, sizeof(Object));
    player->routine = 2;
    oscillatory.direction = 1 << (15 - 8); // entry 8 heading down
    // rate 0x0000 - 2 = -2, value $5100 + (-2) = $50FE: byte $50 == amplitude, stay down
    oscillatory.state[8][0] = 0x5100;
    oscillatory.state[8][1] = 0x0000;
    OscillateNumDo();
    CHECK((oscillatory.direction & (1 << (15 - 8))) != 0);
    // value byte $4F < amplitude $50 -> back to up
    oscillatory.state[8][0] = 0x4F10;
    oscillatory.state[8][1] = 0x0002;
    OscillateNumDo();
    CHECK((oscillatory.direction & (1 << (15 - 8))) == 0);
}

void RegisterOscillatorTests(void) {
    RUN_TEST(Oscillator_FlipsDownWhenValueEqualsAmplitude);
    RUN_TEST(Oscillator_FlipsUpWhenValueBelowAmplitude);
}
