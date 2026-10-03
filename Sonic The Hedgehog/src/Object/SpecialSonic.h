#ifndef _SPECIALSONIC_H
#define _SPECIALSONIC_H

#include "Object.h"

// Special stage Sonic (object 09). The original keeps pointers into the layout RAM; here they are indexes into
// ss_layout.
typedef struct {
    uint8_t touched_id;      // ID of the solid block found by the latest wall check (0 = none)
    uint8_t timeout_updown;  // frames before an UP/DOWN block can act again
    uint8_t timeout_r;       // frames before an R block can act again
    uint8_t ghost_state;     // 0 = idle, 1 = a ghost block has been passed, 2 = ...and then the invisible trigger
    uint16_t exit_timer;     // (unused secondary exit routine)
    uint8_t pad[2];
    uint32_t touched_index;  // layout index of that block
} Scratch_SpecialSonic;

void Obj_SpecialSonic(Object *obj);

#endif // _SPECIALSONIC_H
