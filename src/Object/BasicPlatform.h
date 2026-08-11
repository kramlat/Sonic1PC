#ifndef _BASICPLATFORM_H
#define _BASICPLATFORM_H

#include "Object.h"

// Basic platform assets -- shared object, one mapping set per zone that
// places it (GHZ/SYZ/SLZ)
#include "Resource/Mappings/GHZPlatforms.h"
#include "Resource/Mappings/SYZPlatforms.h"
#include "Resource/Mappings/SLZPlatforms.h"

typedef struct {
    uint8_t subtype;  // 0x28 -- movement type; set by the level's own object-placement data (or debug mode) before routine 0 ever runs
    uint8_t pad0[3];  // 0x29-0x2B
    dword_s raw_y;    // 0x2C -- Y position without the standing-on nudge offset applied, full 32-bit fixed point (only the Falling subtype needs the fractional part, but real hardware allocates it as a long either way)
    int16_t orig_x;   // 0x32 -- initial X position, oscillation center
    int16_t orig_y;   // 0x34 -- initial Y position, oscillation center
    uint8_t nudge;    // 0x38 -- 0-0x40, how far down Sonic has pressed the platform by standing on it
    uint8_t pad1;     // 0x39
    int16_t delay;    // 0x3A -- multi-purpose delay timer (FallAfterStand/FallingDown/RiseOnSwitch)
} Scratch_BasicPlatform;

void Obj_BasicPlatform(Object *obj);

#endif //_BASICPLATFORM_H
