#ifndef _YADRIN_H
#define _YADRIN_H

#include "Object.h"

typedef struct {
    uint8_t pad0[8]; // 0x28-0x2F
    int16_t timedelay; // 0x30 -- delay before turning around
} Scratch_Yadrin;

void Obj_Yadrin(Object *obj);

#endif //_YADRIN_H
