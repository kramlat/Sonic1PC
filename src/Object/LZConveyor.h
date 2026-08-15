#ifndef _LZCONVEYOR_H
#define _LZCONVEYOR_H

#include "Object.h"

typedef struct {
    int16_t x, y;
} LCon_Point;

typedef struct {
    int8_t groupid;   // copy of obSubtype from the initial group spawner (children never write this, stays 0)
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

void Obj_LabyrinthConvey(Object *obj);

// Shared with Object 6F (SBZ spin platform conveyor) -- real hardware calls
// this exact same subroutine from both objects.
void LCon_ChangeDir(Object *obj, Scratch_LCon *scratch);

#endif //_LZCONVEYOR_H
