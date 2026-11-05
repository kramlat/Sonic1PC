#ifndef _SCRAPSTOMP_H
#define _SCRAPSTOMP_H

#include "Sst.h"

typedef struct {
    int16_t orig_y;       // 0x30
    int16_t orig_x;           // 0x34
    int16_t delay;              // 0x36 -- delay timer before moving again
    uint8_t active;                // 0x38 -- flag set when a switch is pressed
    int16_t offset_now;                // 0x3A -- current X/Y-offset from origin
    int16_t offset_max;                  // 0x3C -- maximum move distance from origin
    uint8_t switch_id;                     // 0x3E -- switch ID that triggers platform behavior
} Own_ScrapStomp;

void Obj_ScrapStomp(Sst *self);

#endif //_SCRAPSTOMP_H
