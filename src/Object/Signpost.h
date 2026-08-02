#ifndef _SIGNPOST_H
#define _SIGNPOST_H

#include "Object.h"

// Signpost assets
#include "Resource/Animation/Signpost.h"
#include "Resource/Mappings/Signpost.h"

#ifdef SCP_REV00
extern const uint8_t Mappings_RingREV00[]; // From Ring.c
#else
extern const uint8_t Mappings_RingREV01[]; // From Ring.c
#endif

typedef struct {
    uint8_t pad[8]; // 0x28-0x2F
    int16_t spin_time; // 0x30
    int16_t sparkle_time; // 0x32
    uint8_t sparkle_id; // 0x34
} Scratch_Signpost;

#endif //_SIGNPOST_H
