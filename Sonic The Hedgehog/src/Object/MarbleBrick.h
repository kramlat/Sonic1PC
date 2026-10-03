#ifndef _MARBLEBRICK_H
#define _MARBLEBRICK_H

#include "Object.h"

// Marble brick assets
#include "Resource/Mappings/MZBricks.h"

typedef struct {
    uint8_t pad0[8]; // 0x28-0x2F
    int16_t orig_y;  // 0x30 -- initial Y-position, used by the wobble effect
} Scratch_MarbleBrick;

void Obj_MarbleBrick(Object *obj);

#endif //_MARBLEBRICK_H
