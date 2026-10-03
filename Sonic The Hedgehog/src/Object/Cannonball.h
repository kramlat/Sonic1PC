#ifndef _CANNONBALL_H
#define _CANNONBALL_H

#include "Object.h"

typedef struct {
    uint8_t pad0[8]; // 0x28-0x2F (0x28 = subtype, copied from Ball Hog's own explosion timer)
    int16_t time;       // 0x30 -- frames until the cannonball explodes
    uint8_t clear;      // 0x32 -- set once the ball has been out in open air (see CBal_BounceOffWalls)
} Scratch_Cannonball;

void Obj_Cannonball(Object *obj);

#endif //_CANNONBALL_H
