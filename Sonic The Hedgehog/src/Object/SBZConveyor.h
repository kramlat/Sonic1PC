#ifndef _SBZCONVEYOR_H
#define _SBZCONVEYOR_H

#include "Object.h"

typedef struct {
    uint8_t pad0[14]; // 0x28-0x35 (0x28 = subtype)
    int16_t speed;      // 0x36 -- pixels/frame to push Sonic at (can be negative)
    uint8_t width;         // 0x38 -- half-width of the belt (128 or 56px)
} Scratch_SBZConveyor;

void Obj_SBZConveyor(Object *obj);

#endif //_SBZCONVEYOR_H
