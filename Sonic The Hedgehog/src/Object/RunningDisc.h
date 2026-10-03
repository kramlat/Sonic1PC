#ifndef _RUNNINGDISC_H
#define _RUNNINGDISC_H

#include "Object.h"

// Object 67 - disc that Sonic runs around (SBZ act 2)
//
// While Sonic is attached the disc sets his stick-to-convex flag (Scratch_Sonic x38.floor_clip, the original's sticktoconvex), which his floor
// following (Sonic_AnglePos) and slope repel check, so that he stays on the gear's tight curve; jumping, or leaving the gear, clears it.

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
