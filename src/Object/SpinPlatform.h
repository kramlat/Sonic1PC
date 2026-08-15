#ifndef _SPINPLATFORM_H
#define _SPINPLATFORM_H

#include "Object.h"

typedef struct {
    uint8_t pad0[8];  // 0x28-0x2F (0x28 = subtype)
    int16_t timer;       // 0x30 -- counter for time until event
    int16_t timelen;        // 0x32 -- time between changes (general)
    uint8_t spinning;         // 0x34 -- flag set while platform is spinning
    uint8_t pad1;               // 0x35
    uint16_t syncmask;            // 0x36 -- frame-counter sync bitmask to check if platform should start spinning
} Scratch_SpinPlatform;

void Obj_SpinPlatform(Object *obj);

#endif //_SPINPLATFORM_H
