#ifndef _SIDEWAYSSTOMPER_H
#define _SIDEWAYSSTOMPER_H

#include "Sst.h"

typedef struct {
    int16_t orig_x;       // 0x30 -- initial X-position per child object
    uint16_t current_x;   // 0x32 -- current relative X-offset for stomping (8.8 fixed)
    uint16_t length;      // 0x34 -- stomper length
    bool retract;         // 0x36 -- set if stomper is currently slowly retracting
    int16_t delay;        // 0x38 -- delay after stomper has fully extended before retracting
    int16_t orig_x2;      // 0x3A -- initial X-position for parent
    uint8_t parent_index; // 0x3C on real hardware (pointer there) -- children only
} Own_SideStomp;

void Obj_SidewaysStomper(Sst *self);

#endif //_SIDEWAYSSTOMPER_H
