#ifndef _BIGSPIKEBALL_H
#define _BIGSPIKEBALL_H

#include "Object.h"

// BigSpikeBall mappings resource
#include "Resource/Mappings/BigSpikedBall.h"

// Scratch structure mapping the Assembly offsets:
// bball_origY = 0x38, bball_origX = 0x3A, bball_radius = 0x3C, bball_speed = 0x3E
typedef struct {
    int16_t orig_y;    // 0x38
    int16_t orig_x;    // 0x3A
    uint8_t radius;    // 0x3C
    uint8_t unused;    // 0x3D
    int16_t speed;     // 0x3E
} Scratch_BigSpikeBall;

void Obj_BigSpikeBall(Object *obj);

#endif // _BIGSPIKEBALL_H
