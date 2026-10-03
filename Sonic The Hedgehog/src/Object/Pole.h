#ifndef _POLE_H
#define _POLE_H

#include "Object.h"

typedef struct {
    uint8_t pad0[8];    // 0x28-0x2F
    int16_t breaktime;  // 0x30 -- time between grabbing the pole and it breaking
    bool grabbed;       // 0x32 -- flag set while Sonic grabs the pole
} Scratch_Pole;

void Obj_Pole(Object *obj);

#endif //_POLE_H
