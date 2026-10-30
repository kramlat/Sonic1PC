#ifndef _GRASSFIRE_H
#define _GRASSFIRE_H

#include "Sst.h"

typedef struct {
    int16_t orig_x;    // 0x2A
    int16_t orig_y;     // 0x2C -- (children only) initial slope-aligned Y, copied from parent at spawn time
    int16_t nudge;       // 0x3C -- current platform depression distance, copied from parent
    uint8_t platform;      // 0x38 -- pointer to parent LargeGrass platform object
} Own_GrassFire;

void Obj_GrassFire(Sst *self);

#endif //_GRASSFIRE_H
