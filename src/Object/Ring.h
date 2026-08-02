#ifndef _RING_H
#define _RING_H

#include "Object.h"

// Ring assets
#include "Resource/Animation/Ring.h"
#ifdef SCP_REV00
#include "Resource/Mappings/RingREV00.h"
#else
#include "Resource/Mappings/RingREV01.h"
#endif

typedef struct {
    uint8_t subtype; // 0x28
    uint8_t pad[0x9]; // 0x29-0x31
    int16_t base_x; // 0x32
    uint8_t index; // 0x34
} Scratch_Ring;

#endif //_RING_H
