#ifndef _LZCONVEYOR_H
#define _LZCONVEYOR_H

#include "Object.h"

typedef struct {
    int16_t x, y;
} LCon_Point;

typedef struct {
    uint8_t pad0[7];  // 0x28-0x2E
    int8_t groupid;   // 0x2F -- copy of obSubtype from the initial group spawner (children never write this, stays 0)
    int16_t base_x;   // 0x30 -- base X-position for entire group (roughly in the center), used for out-of-range check
    uint8_t pad1[2];  // 0x32-0x33
    int16_t next_x;   // 0x34 -- next target X-position for platform
    int16_t next_y;   // 0x36 -- next target Y-position for platform
    uint8_t posindex; // 0x38 -- current index in target positioning data
    uint8_t count;    // 0x39 -- number of entries in group's corner data
    int8_t increment; // 0x3A -- +1 or -1
    bool reversed;    // 0x3B -- flag set if conveyor direction is currently reversed
    const LCon_Point *points; // 0x3C -- corner data for group
} Scratch_LCon;

void Obj_LabyrinthConvey(Object *obj);

// Shared with Object 6F (SBZ spin platform conveyor) -- real hardware calls
// this exact same subroutine from both objects.
void LCon_ChangeDir(Object *obj, Scratch_LCon *scratch);

#endif //_LZCONVEYOR_H
