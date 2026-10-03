#ifndef _CIRCLINGPLATFORM_H
#define _CIRCLINGPLATFORM_H

#include "Object.h"

typedef struct {
    uint8_t pad0[8]; // 0x28-0x2F (0x28 = subtype, see Object 5A's comment block for its bitfield meaning)
    int16_t orig_y;    // 0x30
    int16_t orig_x;     // 0x32
} Scratch_CirclingPlatform;

void Obj_CirclingPlatform(Object *obj);

#endif //_CIRCLINGPLATFORM_H
