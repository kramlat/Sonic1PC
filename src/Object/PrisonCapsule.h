#ifndef _PRISONCAPSULE_H
#define _PRISONCAPSULE_H

#include "Object.h"

typedef struct {
    uint8_t pad0[8]; // 0x28-0x2F (0x28 = subtype)
    int16_t orig_y;     // 0x30
} Scratch_PrisonCapsule;

void Obj_PrisonCapsule(Object *obj);

#endif //_PRISONCAPSULE_H
