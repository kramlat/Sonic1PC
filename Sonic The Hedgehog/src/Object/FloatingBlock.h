#ifndef _FLOATINGBLOCK_H
#define _FLOATINGBLOCK_H

#include "Sst.h"

typedef struct {
    int16_t orig_y;  // 0x30 -- original y-axis position
    int16_t orig_x;  // 0x34 -- original x-axis position
    bool moving;     // 0x38 -- flag set if object is currently moving
    int16_t distance; // 0x3A -- total distance to move
    uint8_t switch_id; // 0x3C -- switch ID that triggers action behavior
} Own_FloatingBlock;

void Obj_FloatingBlock(Sst *self);

#endif //_FLOATINGBLOCK_H
