#ifndef _FLAPDOOR_H
#define _FLAPDOOR_H

#include "Sst.h"

typedef struct {
    int16_t wait;    // 0x30 -- time until change (in multiples of 60 frames)
    int16_t time;    // 0x32 -- time between opening/closing
} Own_FlapDoor;

void Obj_FlapDoor(Sst *self);

#endif //_FLAPDOOR_H
