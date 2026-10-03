#ifndef _ROTATINGJUNCTION_H
#define _ROTATINGJUNCTION_H

#include "Object.h"

typedef struct {
    uint8_t pad0[10]; // 0x28-0x31 (0x28 = subtype; 0x30 = jun_unused, set to 60 but never read)
    uint8_t grabframe;  // 0x32 -- frame ID that triggered Sonic getting grabbed
    uint8_t pad1;         // 0x33
    int8_t direction;       // 0x34 -- current rotation direction (1 = clockwise, -1 = counterclockwise)
    uint8_t pad2;             // 0x35
    uint8_t switchdown;         // 0x36 -- flag set while the reversal switch is held down
    uint8_t pad3;                 // 0x37
    uint8_t switch_id;               // 0x38 -- which switch ID reverses the disc
} Scratch_Junction;

void Obj_RotatingJunction(Object *obj);

#endif //_ROTATINGJUNCTION_H
