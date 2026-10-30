#ifndef _LAVAWALL_H
#define _LAVAWALL_H

#include "Sst.h"

typedef struct {
    bool moving;          // 0x36 -- flag set when lava wall is moving
    uint8_t parent_index; // 0x3C on real hardware (pointer there) -- child only
} Own_LavaWall;

void Obj_LavaWall(Sst *self);

#endif //_LAVAWALL_H
