#ifndef _CHARCONTROL_H
#define _CHARCONTROL_H

#include "Object.h"
#include "Level.h"

// The prototype's per-character "object control" byte ($2A of Sonic's and of Tails' object): bit 0 stops his own movement, bit 7 switches his collisions with objects off. An object that carries one
// of them (Chemical Plant's tubes) sets it for that character only. It lives at offset $2E of the character's scratch memory here ($2A is the tumble counter in Nick Arcade's Sonic). Sonic 1's
// global lock_multi (Sonic only) still counts for Sonic.
#define OBJ_CONTROL(chr) ((chr)->scratch.u8[0x2E - 0x28])

static inline uint8_t CharObjControl(const Object *chr) {
    return (uint8_t)(OBJ_CONTROL(chr) | (chr == player ? lock_multi : 0));
}

#endif //_CHARCONTROL_H
