#ifndef _SWINGINGPLATFORM_H
#define _SWINGINGPLATFORM_H

#include "Object.h"

// Define the scratch memory layout to match the ASM object offsets
typedef struct {
    uint8_t  child_count;    // 0x28 (obSubtype) - Number of chain links
    uint8_t  child_idx[15];  // 0x29+ - List of object indices for links (15 is already far more than any real chain uses -- keeps the struct within the 24-byte scratch budget, see below)
    int16_t  orig_y;         // 0x38 (objoff_38)
    int16_t  orig_x;         // 0x3A (objoff_3A)
    uint8_t  chain_length;   // 0x3C (objoff_3C) - Distance from anchor
    uint8_t  direction;      // 0x3D (objoff_3D) -- GHZ wrecking ball boss only
    int16_t  speed;          // 0x3E (objoff_3E) -- GHZ wrecking ball boss only
} Scratch_Swing;

// Mappings (These should be defined in your Resource headers)
#ifndef SwingingPlatform_Build
extern const uint8_t Mappings_SwingGHZ[];
extern const uint8_t Mappings_SwingSLZ[];
extern const uint8_t Mappings_BallGHZ[];
#endif

// Shared with Object 0x48 (GHZ boss wrecking ball) -- real hardware calls
// this exact same subroutine (Swing_UpdateSwingPosition) from both objects.
void Obj_SwingingPlatform_Move2(Object *obj, int16_t sin, int16_t cos);

#endif //_SWINGINGPLATFORM_H
