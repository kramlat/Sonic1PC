#ifndef _BIGSPIKEBALL_H
#define _BIGSPIKEBALL_H

#include "Sst.h"

// BigSpikeBall mappings resource

// Scratch structure mapping the Assembly offsets:
// obSubtype = 0x28, bball_origY = 0x38, bball_origX = 0x3A, bball_radius = 0x3C, bball_speed = 0x3E
typedef struct {
    int16_t orig_y;     // 0x38
    int16_t orig_x;     // 0x3A
    uint8_t radius;     // 0x3C
    uint8_t angle_frac; // 0x3D -- sub-angle accumulator -- see Obj_BigSpikeBall_Type03 for why this is needed
    int16_t speed;      // 0x3E
} Own_BigSpikeBall;

void Obj_BigSpikeBall(Sst *self);

#endif // _BIGSPIKEBALL_H
