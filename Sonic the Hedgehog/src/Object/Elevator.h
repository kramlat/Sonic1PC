#ifndef _ELEVATOR_H
#define _ELEVATOR_H

#include "Sst.h"

typedef struct {
    int16_t orig_y;           // 0x30
    int16_t orig_x;            // 0x32
    int32_t moved_distance;     // 0x34 -- 16.16 fixed-point distance moved from origin so far
    int16_t acceleration;        // 0x38 -- current acceleration per frame while moving
    uint8_t slowing_down;         // 0x3A -- set when moving platform is slowing down again
    int16_t half_distance;          // 0x3C -- half of target distance to move (also elev_spawner_delay for spawner subtype)
    int16_t spawner_delaybase;       // 0x3E -- spawner subtype only: base value for half_distance/spawner_delay
} Own_Elevator;

void Obj_Elevator(Sst *self);

#endif //_ELEVATOR_H
