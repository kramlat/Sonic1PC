#ifndef _BOMB_H
#define _BOMB_H

#include "Object.h"

typedef struct {
    uint8_t pad0[8];  // 0x28-0x2F (0x28 = subtype, read directly via obj->scratch.u8[0])
    int16_t time;      // 0x30 -- multi-purpose timer (walking, waiting, fuse)
    uint8_t pad1[2];    // 0x32-0x33
    int16_t orig_y;      // 0x34 -- fuse's original Y-position, independent of it moving
} Scratch_Bomb;

void Obj_Bomb(Object *obj);

#endif //_BOMB_H
