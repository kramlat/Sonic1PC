#ifndef _FLAMETHROWER_H
#define _FLAMETHROWER_H

#include "Object.h"

typedef struct {
    uint8_t pad0[8];   // 0x28-0x2F (0x28 = subtype)
    int16_t timer;       // 0x30 -- current timer value
    int16_t firetime;      // 0x32 -- base time for the flamethrower to fire
    int16_t pausetime;       // 0x34 -- base time for the flamethrower to idle
    uint8_t hurtframe;         // 0x36 -- frame ID that's harmful to Sonic
} Scratch_Flamethrower;

void Obj_Flamethrower(Object *obj);

#endif //_FLAMETHROWER_H
