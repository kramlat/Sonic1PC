#ifndef _GIRDERBLOCK_H
#define _GIRDERBLOCK_H

#include "Sst.h"

typedef struct {
    int16_t orig_y;      // 0x30
    int16_t orig_x;        // 0x32
    int16_t time;            // 0x34 -- duration for movement in the current direction (in frames)
    uint8_t set;                 // 0x38 -- which of the 4 movement settings to use next (0/8/0x10/0x18)
    int16_t delay;                   // 0x3A -- delay before starting the next movement
} Own_GirderBlock;

void Obj_GirderBlock(Sst *self);

#endif //_GIRDERBLOCK_H
