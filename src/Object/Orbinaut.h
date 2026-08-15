#ifndef _ORBINAUT_H
#define _ORBINAUT_H

#include "Object.h"

typedef struct {
    uint8_t pad0[14];     // 0x28-0x35
    int8_t circledir;     // 0x36 -- circling direction for spikeballs, parent only (1 clockwise, -1 counter clockwise)
    uint8_t ammo;         // 0x37 -- number of not-fired spikeballs, parent only
    uint8_t ball_idx[4];  // 0x38-0x3B -- object indices for spikeballs, parent only
    uint8_t parent_index; // 0x3C on real hardware (pointer there) -- spikeballs only
} Scratch_Orbinaut;

void Obj_Orbinaut(Object *obj);

#endif //_ORBINAUT_H
