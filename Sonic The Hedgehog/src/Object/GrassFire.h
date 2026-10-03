#ifndef _GRASSFIRE_H
#define _GRASSFIRE_H

#include "Object.h"

typedef struct {
    uint8_t pad0[2];  // 0x28-0x29 (0x28 = subtype, read directly via obj->scratch.u8[0] -- 0 for the parent flame, 1 for children, skipping GFire_Spread)
    int16_t orig_x;    // 0x2A
    int16_t orig_y;     // 0x2C -- (children only) initial slope-aligned Y, copied from parent at spawn time
    int16_t nudge;       // 0x3C -- current platform depression distance, copied from parent
    Object *platform;      // 0x38 -- pointer to parent LargeGrass platform object
} Scratch_GrassFire;

void Obj_GrassFire(Object *obj);

#endif //_GRASSFIRE_H
