#ifndef _SMASHWALL_H
#define _SMASHWALL_H

#include "Sst.h"


typedef struct {
    int16_t speed;    // 0x30 -- backup of Sonic's horizontal speed right before hitting the wall (SolidObject can change it)
} Own_SmashWall;

void Obj_SmashWall(Sst *self);

#endif //_SMASHWALL_H
