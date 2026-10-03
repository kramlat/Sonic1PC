#ifndef _FLOATINGBLOCK_H
#define _FLOATINGBLOCK_H

#include "Object.h"

typedef struct {
    uint8_t pad0[8]; // 0x28-0x2F
    int16_t orig_y;  // 0x30 -- original y-axis position
    uint8_t pad1[2]; // 0x32-0x33
    int16_t orig_x;  // 0x34 -- original x-axis position
    uint8_t pad2[2]; // 0x36-0x37
    bool moving;     // 0x38 -- flag set if object is currently moving
    uint8_t pad3;    // 0x39
    int16_t distance; // 0x3A -- total distance to move
    uint8_t switch_id; // 0x3C -- switch ID that triggers action behavior
} Scratch_FloatingBlock;

void Obj_FloatingBlock(Object *obj);

#endif //_FLOATINGBLOCK_H
