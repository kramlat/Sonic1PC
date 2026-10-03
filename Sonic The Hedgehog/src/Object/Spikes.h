#ifndef _SPIKES_H
#define _SPIKES_H

#include "Object.h"

// Spikes assets
#include "Resource/Mappings/Spikes.h"

typedef struct {
    uint8_t subtype; // 0x28
    uint8_t pad1[0x7]; // 0x29-0x2F
    int16_t orig_x; // 0x30
    int16_t orig_y; // 0x32
    word_u move; // 0x34
    uint16_t dir; // 0x36
    uint16_t timer; // 0x38
} Scratch_Spikes;

#endif //_SPIKES_H
