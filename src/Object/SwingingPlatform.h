#ifndef _SWINGINGPLATFORM_H
#define _SWINGINGPLATFORM_H

#include "Object.h"

// Define the scratch memory layout to match the ASM object offsets
typedef struct {
    uint8_t  child_count;    // 0x28 (obSubtype) - Number of chain links
    uint8_t  child_idx[16];  // 0x29+ - List of object indices for links
    int16_t  orig_y;         // 0x38 (objoff_38)
    int16_t  orig_x;         // 0x3A (objoff_3A)
    uint8_t  chain_length;   // 0x3C (objoff_3C) - Distance from anchor
    uint8_t  direction;      // 0x3D (objoff_3D)
    int16_t  speed;          // 0x3E (objoff_3E)
} Scratch_Swing;

// Mappings (These should be defined in your Resource headers)
#ifndef SwingingPlatform_Build
extern const uint8_t Mappings_SwingGHZ[];
extern const uint8_t Mappings_SwingSLZ[];
extern const uint8_t Mappings_BallGHZ[];
#endif

#endif //_SWINGINGPLATFORM_H
