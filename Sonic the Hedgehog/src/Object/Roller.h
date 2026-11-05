#ifndef _ROLLER_H
#define _ROLLER_H

#include "Sst.h"

typedef struct {
    int16_t wait_unfolded; // 0x30 -- frames to wait in destroyable, unfolded state
    uint8_t stateflags;    // 0x32 -- bit 0 set if hit a ledge before, bit 7 set if Roller has unfolded before
} Own_Roller;

void Obj_Roller(Sst *self);

#endif //_ROLLER_H
