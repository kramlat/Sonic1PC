#ifndef _TELEPORTER_H
#define _TELEPORTER_H

#include "Sst.h"

// Object 72 - invisible teleporter system inside tubes (SBZ act 2)
typedef struct {
    int16_t time;         // 0x2E -- remaining time to move in current direction
    uint8_t prebump;           // 0x32 -- pre-bump value before Sonic gets shot off (increments by 2, triggers at 0x80)
    int16_t target_x;               // 0x36 -- next X-position target
    int16_t target_y;                  // 0x38 -- next Y-position target
    uint8_t current;                      // 0x3A -- current index into the group's target data
} Own_Teleport;

void Obj_Teleporter(Sst *self);

#endif //_TELEPORTER_H
