#ifndef _SMASHWALL_H
#define _SMASHWALL_H

#include "Object.h"

// Smashable wall assets
#include "Resource/Mappings/SmashableWalls.h"

typedef struct {
    uint8_t pad0[8];  // 0x28-0x2F
    int16_t speed;    // 0x30 -- backup of Sonic's horizontal speed right before hitting the wall (SolidObject can change it)
} Scratch_SmashWall;

void Obj_SmashWall(Object *obj);

#endif //_SMASHWALL_H
