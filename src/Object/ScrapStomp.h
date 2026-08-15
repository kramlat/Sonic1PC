#ifndef _SCRAPSTOMP_H
#define _SCRAPSTOMP_H

#include "Object.h"

typedef struct {
    uint8_t pad0[8];   // 0x28-0x2F (0x28 = subtype, see Object 6B's own comment block)
    int16_t orig_y;       // 0x30
    uint8_t pad1[2];        // 0x32-0x33
    int16_t orig_x;           // 0x34
    int16_t delay;              // 0x36 -- delay timer before moving again
    uint8_t active;                // 0x38 -- flag set when a switch is pressed
    uint8_t pad2;                    // 0x39
    int16_t offset_now;                // 0x3A -- current X/Y-offset from origin
    int16_t offset_max;                  // 0x3C -- maximum move distance from origin
    uint8_t switch_id;                     // 0x3E -- switch ID that triggers platform behavior
} Scratch_ScrapStomp;

void Obj_ScrapStomp(Object *obj);

#endif //_SCRAPSTOMP_H
