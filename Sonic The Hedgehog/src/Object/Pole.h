#ifndef _POLE_H
#define _POLE_H

#include "Sst.h"

typedef struct {
    int16_t breaktime;  // 0x30 -- time between grabbing the pole and it breaking
    bool grabbed;       // 0x32 -- flag set while Sonic grabs the pole
} Own_Pole;

void Obj_Pole(Sst *self);

#endif //_POLE_H
