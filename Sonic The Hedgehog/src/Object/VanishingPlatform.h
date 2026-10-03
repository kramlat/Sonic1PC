#ifndef _VANISHINGPLATFORM_H
#define _VANISHINGPLATFORM_H

#include "Object.h"

// Vanishing platform assets
#include "Resource/Mappings/VanishingPlatforms.h"
#include "Resource/Animation/VanishingPlatforms.h"

typedef struct {
    uint8_t subtype;      // 0x28 (obSubtype)
    uint8_t pad0[7];      // 0x29-0x2F
    int16_t timer;        // 0x30 (vanp_timer)
    int16_t timelen;      // 0x32 (vanp_timelen)
    uint8_t pad1[2];      // 0x34-0x35
    uint16_t syncoffset;  // 0x36 (vanp_syncoffset)
    uint16_t syncmask;    // 0x38 (vanp_syncmask)
} Scratch_VanishPlatform;

void Obj_VanishPlatform(Object *obj);

#endif //_VANISHINGPLATFORM_H
