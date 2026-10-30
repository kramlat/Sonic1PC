#ifndef _PUSHBLOCK_H
#define _PUSHBLOCK_H

#include "Sst.h"

typedef struct {
    int16_t lava_speed; // 0x30 -- X-speed remembered from just before landing on lava
    bool on_lava;          // 0x32
    int16_t orig_x;           // 0x34
    int16_t orig_y;              // 0x36
} Own_PushBlock;

void Obj_PushBlock(Sst *self);

#endif //_PUSHBLOCK_H
