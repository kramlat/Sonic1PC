#ifndef _BOMB_H
#define _BOMB_H

#include "Sst.h"

typedef struct {
    int16_t time;      // 0x30 -- multi-purpose timer (walking, waiting, fuse)
    int16_t orig_y;      // 0x34 -- fuse's original Y-position, independent of it moving
} Own_Bomb;

void Obj_Bomb(Sst *self);

#endif //_BOMB_H
