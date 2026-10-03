#ifndef _TELEPORTER_H
#define _TELEPORTER_H

#include "Object.h"

// Object 72 - invisible teleporter system inside tubes (SBZ act 2)
typedef struct {
    uint8_t pad0[6];   // 0x28-0x2D (0x28 = subtype -- also doubles as the Tele_Data group index throughout; never modified)
    int16_t time;         // 0x2E -- remaining time to move in current direction
    uint8_t pad1[2];        // 0x30-0x31
    uint8_t prebump;           // 0x32 -- pre-bump value before Sonic gets shot off (increments by 2, triggers at 0x80)
    uint8_t pad2[3];             // 0x33-0x35
    int16_t target_x;               // 0x36 -- next X-position target
    int16_t target_y;                  // 0x38 -- next Y-position target
    uint8_t current;                      // 0x3A -- current index into the group's target data
} Scratch_Teleport;

void Obj_Teleporter(Object *obj);

#endif //_TELEPORTER_H
