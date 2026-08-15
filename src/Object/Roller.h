#ifndef _ROLLER_H
#define _ROLLER_H

#include "Object.h"

typedef struct {
    uint8_t pad0[8];       // 0x28-0x2F
    int16_t wait_unfolded; // 0x30 -- frames to wait in destroyable, unfolded state
    uint8_t stateflags;    // 0x32 -- bit 0 set if hit a ledge before, bit 7 set if Roller has unfolded before
} Scratch_Roller;

void Obj_Roller(Object *obj);

#endif //_ROLLER_H
