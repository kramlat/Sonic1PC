#ifndef _LAVABALL_H
#define _LAVABALL_H

#include "Object.h"

typedef struct {
    uint8_t pad0;      // 0x28 (subtype, read directly via obj->scratch.u8[0])
    uint8_t from_boss;  // 0x29 -- set if spawned from the MZ boss's own lava pit
                        // (lower sprite priority); nothing in this port sets this
                        // yet since the MZ boss isn't ported
    uint8_t pad1[6];      // 0x2A-0x2F
    int16_t orig_y;         // 0x30 -- initial Y-position, for rise-and-fall types
} Scratch_LavaBall;

void Obj_LavaBall(Object *obj);

#endif //_LAVABALL_H
