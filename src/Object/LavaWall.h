#ifndef _LAVAWALL_H
#define _LAVAWALL_H

#include "Object.h"

typedef struct {
    uint8_t pad0[14];     // 0x28-0x35
    bool moving;          // 0x36 -- flag set when lava wall is moving
    uint8_t pad1[5];      // 0x37-0x3B
    uint8_t parent_index; // 0x3C on real hardware (pointer there) -- child only
} Scratch_LavaWall;

void Obj_LavaWall(Object *obj);

#endif //_LAVAWALL_H
