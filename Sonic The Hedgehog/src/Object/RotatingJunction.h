#ifndef _ROTATINGJUNCTION_H
#define _ROTATINGJUNCTION_H

#include "Sst.h"

typedef struct {
    uint8_t grabframe;  // 0x32 -- frame ID that triggered Sonic getting grabbed
    int8_t direction;       // 0x34 -- current rotation direction (1 = clockwise, -1 = counterclockwise)
    uint8_t switchdown;         // 0x36 -- flag set while the reversal switch is held down
    uint8_t switch_id;               // 0x38 -- which switch ID reverses the disc
} Own_Junction;

void Obj_RotatingJunction(Sst *self);

#endif //_ROTATINGJUNCTION_H
