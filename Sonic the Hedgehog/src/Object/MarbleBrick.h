#ifndef _MARBLEBRICK_H
#define _MARBLEBRICK_H

#include "Sst.h"


typedef struct {
    int16_t orig_y;  // 0x30 -- initial Y-position, used by the wobble effect
} Own_MarbleBrick;

void Obj_MarbleBrick(Sst *self);

#endif //_MARBLEBRICK_H
