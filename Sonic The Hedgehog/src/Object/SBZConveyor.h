#ifndef _SBZCONVEYOR_H
#define _SBZCONVEYOR_H

#include "Sst.h"

typedef struct {
    int16_t speed;      // 0x36 -- pixels/frame to push Sonic at (can be negative)
    uint8_t width;         // 0x38 -- half-width of the belt (128 or 56px)
} Own_SBZConveyor;

void Obj_SBZConveyor(Sst *self);

#endif //_SBZCONVEYOR_H
