#ifndef _VANISHINGPLATFORM_H
#define _VANISHINGPLATFORM_H

#include "Sst.h"


typedef struct {
    int16_t timer;        // 0x30 (vanp_timer)
    int16_t timelen;      // 0x32 (vanp_timelen)
    uint16_t syncoffset;  // 0x36 (vanp_syncoffset)
    uint16_t syncmask;    // 0x38 (vanp_syncmask)
} Own_VanishPlatform;

void Obj_VanishPlatform(Sst *self);

#endif //_VANISHINGPLATFORM_H
