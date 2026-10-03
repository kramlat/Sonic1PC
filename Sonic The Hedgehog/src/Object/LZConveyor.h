#ifndef _LZCONVEYOR_H
#define _LZCONVEYOR_H

#include "Object.h"
#include "Macros.h"

#include <stddef.h>

typedef struct {
    int16_t x, y;
} LCon_Point;

typedef struct {
    uint8_t subtype;  // obSubtype (0x28) -- the spawner rewrites its own to the first platform's subtype
    // Copy of obSubtype from the initial group spawner (real ASM's
    // lcon_groupid, objoff_2F -- its own byte, NOT the subtype's). It must
    // survive the spawner turning itself into the first platform, so that
    // platform can clear the group's "loaded" flag when it goes out of
    // range. It used to alias the subtype byte and got overwritten, so a
    // group that scrolled away never respawned.
    int8_t groupid;
    int16_t base_x;   // base X-position for entire group (roughly in the center), used for out-of-range check
    int16_t next_x;   // next target X-position for platform
    int16_t next_y;   // next target Y-position for platform
    uint8_t posindex; // current index in target positioning data
    uint8_t count;    // number of entries in group's corner data
    int8_t increment; // +1 or -1
    bool reversed;    // flag set if conveyor direction is currently reversed
    // `points` is a host pointer (not part of real hardware's scratch
    // layout, which has no room for one) -- its own 8-byte alignment
    // requirement is why the explicit real-offset padding above was
    // dropped: with it, this struct overflowed the 24-byte scratch budget.
    const LCon_Point *points; // corner data for group
} Scratch_LCon;

STATIC_ASSERT(sizeof(Scratch_LCon) <= sizeof(((Object *)0)->scratch), "Scratch_LCon must fit the object scratch area");
STATIC_ASSERT(offsetof(Scratch_LCon, groupid) != offsetof(Scratch_LCon, subtype), "groupid must not alias the subtype byte");

void Obj_LabyrinthConvey(Object *obj);

// Shared with Object 6F (SBZ spin platform conveyor) -- real hardware calls
// this exact same subroutine from both objects.
void LCon_ChangeDir(Object *obj, Scratch_LCon *scratch);

#endif //_LZCONVEYOR_H
