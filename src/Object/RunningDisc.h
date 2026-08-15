#ifndef _RUNNINGDISC_H
#define _RUNNINGDISC_H

#include "Object.h"

// Object 67 - disc that Sonic runs around (SBZ act 2)
//
// NOTE: real hardware also sets a per-frame `sticktoconvex` flag on Sonic
// himself while attached, consumed by 4 separate call sites inside Sonic's
// own AnglePos in-air gating (Sonic_WalkVertR/WalkCeiling/WalkVertL/etc, in
// `_incObj/Sonic AnglePos.asm`) to stop him being kicked into the air while
// running around the gear's tight convex curve at high speed. Sonic's own
// scratch struct is already at its full 24-byte budget, and wiring this
// into 4 separate core movement functions is a standalone task -- deferred
// for now. Normal floor-angle collision should still carry Sonic around
// the gear correctly except at the highest speeds/tightest curvature.

typedef struct {
    uint8_t pad0[8];      // 0x28-0x2F
    int16_t orig_y;          // 0x30
    int16_t orig_x;             // 0x32
    int16_t spot_distance;         // 0x34 -- radius for the small moving spot inside the gear
    int16_t spot_speed;               // 0x36 -- small spot rotation speed (can be negative)
    int16_t triggersize;                 // 0x38 -- trigger distance for Sonic to latch onto the gear
    uint8_t sonic_attached;                // 0x3A -- flag set while Sonic is attached to the gear
    uint8_t angle_frac;                       // sub-angle accumulator -- see Disc_MoveSpot for why this is needed
} Scratch_RunningDisc;

void Obj_RunningDisc(Object *obj);

#endif //_RUNNINGDISC_H
