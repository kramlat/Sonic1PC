#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "Level.h"

typedef struct {
    uint16_t frequency;
    uint16_t amplitude;
} OscillateSettings;

void OscillateNumInit();
void OscillateNumDo();
