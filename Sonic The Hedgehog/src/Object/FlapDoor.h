#ifndef _FLAPDOOR_H
#define _FLAPDOOR_H

#include "Object.h"

typedef struct {
    uint8_t pad0[8]; // 0x28-0x2F
    int16_t wait;    // 0x30 -- time until change (in multiples of 60 frames)
    int16_t time;    // 0x32 -- time between opening/closing
} Scratch_FlapDoor;

void Obj_FlapDoor(Object *obj);

#endif //_FLAPDOOR_H
