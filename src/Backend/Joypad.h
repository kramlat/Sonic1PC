#pragma once

#include <stdint.h>

//Joypad bitmask
#define JPAD_START (1 << 7)
#define JPAD_A     (1 << 6)
#define JPAD_C     (1 << 5)
#define JPAD_B     (1 << 4)
#define JPAD_RIGHT (1 << 3)
#define JPAD_LEFT  (1 << 2)
#define JPAD_DOWN  (1 << 1)
#define JPAD_UP    (1 << 0)

// Extended (non-Genesis) bindings -- real Genesis pads only ever had the 8
// buttons above, all of which are already spoken for in that one byte, so
// anything with no Genesis equivalent (e.g. a modern controller's Y button,
// used for debug mode's "cycle item backward") lives in this separate
// bitmask instead of trying to steal a bit from JPAD_*.
#define JPAD_EXT_Y (1 << 0)
// Debug mode's "adjust selected item's subtype" -- right stick left/right
// (a physical gamepad's right stick specifically, not the D-pad-driven
// movement stick), edge-detected the same way as every other JPAD_* bit
// (Game.c's ReadJoypads already does `state & ~prev_held` generically for
// this whole byte, so reporting "past deadzone" as a held level here is
// enough to get a one-shot bump per deflection, no extra edge-tracking
// needed in the backend itself).
#define JPAD_EXT_SUBTYPE_DEC (1 << 1)
#define JPAD_EXT_SUBTYPE_INC (1 << 2)

//Joupad interface
uint8_t Joypad_GetState1(void);
uint8_t Joypad_GetState2(void);
uint8_t Joypad_GetExtState1(void);
