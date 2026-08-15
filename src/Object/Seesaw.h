#ifndef _SEESAW_H
#define _SEESAW_H

#include "Object.h"

typedef struct {
    uint8_t pad0[8];    // 0x28-0x2F (0x28 = subtype: nonzero = boss-fight seesaw, don't spawn a spikeball)
    int16_t orig_x;       // 0x30
    uint8_t pad1[2];        // 0x32-0x33
    int16_t orig_y;           // 0x34
    int16_t landspeed;          // 0x38 -- seesaw only: Sonic's Y-speed when he landed on it
    int8_t state;                 // 0x3A -- seesaw: 0=descending,1=flat,2=ascending; spikeball: 0=on right,2=on left (never 1)
    uint8_t pad2;                   // 0x3B
    uint8_t parent_index;             // 0x3C on real hardware (pointer there) -- spikeball only: index into objects[] for the parent seesaw
} Scratch_Seesaw;

void Obj_Seesaw(Object *obj);

#endif //_SEESAW_H
