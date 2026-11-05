#ifndef _SEESAW_H
#define _SEESAW_H

#include "Sst.h"

typedef struct {
    int16_t orig_x;       // 0x30
    int16_t orig_y;           // 0x34
    int16_t landspeed;          // 0x38 -- seesaw only: Sonic's Y-speed when he landed on it
    int8_t state;                 // 0x3A -- seesaw: 0=descending,1=flat,2=ascending; spikeball: 0=on right,2=on left (never 1)
    uint8_t parent_index;             // 0x3C on real hardware (pointer there) -- spikeball only: index into sst[] for the parent seesaw
} Own_Seesaw;

void Obj_Seesaw(Sst *self);

#endif //_SEESAW_H
