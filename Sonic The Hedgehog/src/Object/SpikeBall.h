#ifndef _SPIKEBALL_H
#define _SPIKEBALL_H

#include "Object.h"

typedef struct {
    uint8_t pad0;        // 0x28 -- obSubtype, read directly via scratch.u8[0]
    uint8_t children;    // 0x29 -- number of child objects
    uint8_t pad1[6];     // 0x2A-0x2F
    uint8_t child_idx[8]; // 0x30-0x37 -- object indices of children, parent's own index stored last
    int16_t orig_y;      // 0x38 -- centre Y-position
    int16_t orig_x;      // 0x3A -- centre X-position
    uint8_t radius;       // 0x3C
    uint8_t angle_frac;    // 0x3D -- sub-angle accumulator -- see SBall_Twirl for why this is needed
    int16_t speed;           // 0x3E -- rate of spin
} Scratch_SpikeBall;

void Obj_SpikeBall(Object *obj);

#endif //_SPIKEBALL_H
