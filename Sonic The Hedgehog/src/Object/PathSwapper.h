#pragma once

#include "Object.h"

// Object 03 - Path Swapper / Collision Switcher (ported from Sonic 2, via
// s1disasm's ProjectSonic1TwoEight branch's "03 Collision Switcher.asm").
// A stationary, invisible trigger that flips which of the level's two
// collision paths (Level.h's collision_path) is active when Sonic crosses
// it in the "forward" direction, and flips it back on the reverse crossing.
// Also controls Sonic's sprite draw priority (in front of/behind BG),
// independent of whether it also switches the collision path.
typedef struct {
    uint8_t subtype; // 0x28 -- see obSubtype bit layout in the real source
    uint8_t pad0[9]; // 0x29-0x31
    int16_t size;    // pswap_size, 0x32 -- trigger half-size, real data is pre-halved
    uint8_t passed;  // pswap_passed, 0x34
} Scratch_PathSwapper;

void Obj_PathSwapper(Object *obj);
