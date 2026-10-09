#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "Level.h"

typedef struct {
    uint16_t frequency;
    uint16_t amplitude;
} OscillateSettings;

// What differs between the games, the rest of the routines being shared: the control word and the value and rate each oscillator starts at, and the rate of change and amplitude of each. Each game defines it
// (Sonic1OscillatorData.c, Sonic2OscillatorData.c)
typedef struct {
    uint16_t direction;
    uint16_t start[16][2];
    OscillateSettings settings[16];
} OscillatorData;
extern const OscillatorData oscillator_data;

void OscillateNumInit(void);
void OscillateNumDo(void);
