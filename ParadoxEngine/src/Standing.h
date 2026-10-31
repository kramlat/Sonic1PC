#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "Sst.h"

// What a character is standing on, as the characters remember it (Scratch_Sonic::standing_obj, a byte): 0x80 plus a slot of the status table of Sonic 3 & Knuckles (Sst.h); 0 is nothing.
#define STANDING_SST 0x80

static inline uint8_t Standing_IdOfSst(const Sst *obj) { return (uint8_t)(STANDING_SST + (obj - sst)); }

// The part of the object under the character that the animation needs (balancing on the edge): its X, half its width and its touch flag (status bit 7)
typedef struct {
    int16_t x;
    uint8_t width;
    bool flag7;
} StandingInfo;
void Standing_Get(uint8_t id, StandingInfo *out);

// How the object lets go of the character: only the standing flag; that and its secondary routine (Solid_ResetFloor); that and, if its routine is 4, back to 2 (Platform_SetStand)
typedef enum { STANDING_RELEASE_FLAG, STANDING_RELEASE_SOLID, STANDING_RELEASE_PLATFORM } StandingRelease;
void Standing_Release(uint8_t id, StandingRelease how);
