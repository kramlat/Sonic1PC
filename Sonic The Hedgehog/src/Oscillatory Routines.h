#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "Oscillator.h"
#include "Level.h"

// Each game defines its oscillators' table (Sonic1OscillatorData.c, Sonic2OscillatorData.c); the routines are the engine's (Oscillator.h), and the games' own rule is that they stand still while the player is
// dying
extern const OscillatorData oscillator_data;

void OscillateNumInit(void);
void OscillateNumDo(void);
