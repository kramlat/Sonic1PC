#ifndef _GIRDERBLOCK_H
#define _GIRDERBLOCK_H

#include "Object.h"

typedef struct {
    uint8_t pad0[8];  // 0x28-0x2F
    int16_t orig_y;      // 0x30
    int16_t orig_x;        // 0x32
    int16_t time;            // 0x34 -- duration for movement in the current direction (in frames)
    uint8_t pad1[2];           // 0x36-0x37
    uint8_t set;                 // 0x38 -- which of the 4 movement settings to use next (0/8/0x10/0x18)
    uint8_t pad2;                  // 0x39
    int16_t delay;                   // 0x3A -- delay before starting the next movement
} Scratch_GirderBlock;

void Obj_GirderBlock(Object *obj);

#endif //_GIRDERBLOCK_H
