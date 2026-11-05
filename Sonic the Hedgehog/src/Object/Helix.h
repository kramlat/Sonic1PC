#ifndef _HELIX_H
#define _HELIX_H

#include "Sst.h"

typedef struct {
    uint8_t children_count; // 0x28 -- also doubles as the spawn-time "how many spikes" input (from subtype), matching real hardware's helix_children == obSubtype
    uint8_t children[16];   // 0x29-0x38 -- object-array indices of this spike's own child spikes (parent only)
    uint8_t frame_base;     // 0x3E -- this spike's own starting frame ID (offset into the shared rotation counter, different per spike)
} Own_Helix;

void Obj_Helix(Sst *self);

#endif //_HELIX_H
