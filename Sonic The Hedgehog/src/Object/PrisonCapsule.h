#ifndef _PRISONCAPSULE_H
#define _PRISONCAPSULE_H

#include "Sst.h"

typedef struct {
    int16_t orig_y;     // 0x30
    uint16_t timer;     // frames the current step lasts
} Own_PrisonCapsule;

void Obj_PrisonCapsule(Sst *self);

#endif //_PRISONCAPSULE_H
