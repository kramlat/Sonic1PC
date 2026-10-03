#ifndef _LARGEGRASS_H
#define _LARGEGRASS_H

#include "Object.h"

typedef struct {
    uint8_t pad0;      // 0x29 (0x28 = subtype, read directly via obj->scratch.u8[0] -- shape<<4 | movement type at spawn, reduced to just the movement type 0-5 once Main runs)
    int16_t orig_x;      // 0x2A
    int16_t orig_y;        // 0x2C
    uint8_t nudge;           // 0x34 (movement type 5, burnable, only) -- current depression value
    uint8_t burning;           // 0x35 -- set once this platform has started burning
    uint8_t flames_count;        // 0x36 -- number of live child flame objects
    uint8_t flames[8];             // 0x37-0x3E -- object-array indices of child flame objects
} Scratch_LargeGrass;

// Returns the slope collision heightmap for a given platform shape (0 =
// symmetrical hill, 1 = asymmetrical hill, 2 = column/rectangular) --
// shared with GrassFire, which looks up its own parent's heightmap via the
// parent's obj->frame (which stores this same shape ID for the platform's
// entire lifetime).
const uint8_t *LGrass_Heightmap(uint8_t shape);

// Adds a newly-spawned child flame's object-array index to its parent
// platform's list of live children (shared with GrassFire, which calls
// this each time it spawns another spreading flame).
void LGrass_AddChildToList(Object *platform, Object *child);

void Obj_LargeGrass(Object *obj);

#endif //_LARGEGRASS_H
