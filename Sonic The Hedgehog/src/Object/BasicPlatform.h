#ifndef _BASICPLATFORM_H
#define _BASICPLATFORM_H

#include "Sst.h"

// Basic platform assets -- shared object, one mapping set per zone that
// places it (GHZ/SYZ/SLZ)

typedef struct {
    dword_s raw_y;    // 0x2C -- Y position without the standing-on nudge offset applied, full 32-bit fixed point (only the Falling subtype needs the fractional part, but real hardware allocates it as a long either way)
    int16_t orig_x;   // 0x32 -- initial X position, oscillation center
    int16_t orig_y;   // 0x34 -- initial Y position, oscillation center
    uint8_t nudge;    // 0x38 -- 0-0x40, how far down Sonic has pressed the platform by standing on it
    int16_t delay;    // 0x3A -- multi-purpose delay timer (FallAfterStand/FallingDown/RiseOnSwitch)
} Own_BasicPlatform;

void Obj_BasicPlatform(Sst *self);

#endif //_BASICPLATFORM_H
