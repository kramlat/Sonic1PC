#ifndef _LAVABALL_H
#define _LAVABALL_H

#include "Sst.h"

typedef struct {
    uint8_t from_boss;  // 0x29 -- set if spawned from the MZ boss's own lava pit
                        // (lower sprite priority); nothing in this port sets this
                        // yet since the MZ boss isn't ported
    int16_t orig_y;         // 0x30 -- initial Y-position, for rise-and-fall types
} Own_LavaBall;

void Obj_LavaBall(Sst *self);

#endif //_LAVABALL_H
