#pragma once

#include <stdint.h>

// The oscillators of the Sonic games: sixteen values that swing up and down, each with its own rate of change and amplitude, which platforms, saws, the water's surface and other objects read through their high
// byte. They run once a frame; which frames (not while the player is dying, in the games) is the game's rule.

typedef struct {
    uint16_t frequency; // added to the oscillator's rate each frame
    uint16_t amplitude; // the high byte of the value the oscillator turns round at
} OscillateSettings;

// What differs between the games: the control word (a bit for each oscillator, set while it is heading down; oscillator 0 is bit 15), the value and rate each starts at, and its settings
typedef struct {
    uint16_t direction;
    uint16_t start[16][2];
    OscillateSettings settings[16];
} OscillatorData;

// The game's table: the engine has a null one (weak, in NullServices.c) that a game's own definition replaces
extern const OscillatorData oscillator_data;

typedef struct {
    uint16_t direction;
    uint16_t state[16][2]; // value, rate
} Oscillatory;

extern Oscillatory oscillatory;

void Oscillator_Init(const OscillatorData *data);
void Oscillator_Step(const OscillatorData *data);
