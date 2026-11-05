#ifndef _CANNONBALL_H
#define _CANNONBALL_H

#include "Sst.h"

typedef struct {
    int16_t time;       // 0x30 -- frames until the cannonball explodes
    uint8_t clear;      // 0x32 -- set once the ball has been out in open air (see CBal_BounceOffWalls)
} Own_Cannonball;

void Obj_Cannonball(Sst *self);

#endif //_CANNONBALL_H
