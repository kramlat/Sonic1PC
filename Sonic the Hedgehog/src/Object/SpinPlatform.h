#ifndef _SPINPLATFORM_H
#define _SPINPLATFORM_H

#include "Sst.h"

typedef struct {
    int16_t timer;       // 0x30 -- counter for time until event
    int16_t timelen;        // 0x32 -- time between changes (general)
    uint8_t spinning;         // 0x34 -- flag set while platform is spinning
    uint16_t syncmask;            // 0x36 -- frame-counter sync bitmask to check if platform should start spinning
} Own_SpinPlatform;

void Obj_SpinPlatform(Sst *self);

#endif //_SPINPLATFORM_H
