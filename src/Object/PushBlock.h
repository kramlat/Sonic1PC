#ifndef _PUSHBLOCK_H
#define _PUSHBLOCK_H

#include "Object.h"

typedef struct {
    uint8_t pad0[8];      // 0x28-0x2F -- scratch.u8[0] here aliases obSubtype/on-stomper flag
    int16_t lava_speed; // 0x30 -- X-speed remembered from just before landing on lava
    bool on_lava;          // 0x32
    int16_t orig_x;           // 0x34
    int16_t orig_y;              // 0x36
} Scratch_PushBlock;

void Obj_PushBlock(Object *obj);

#endif //_PUSHBLOCK_H
