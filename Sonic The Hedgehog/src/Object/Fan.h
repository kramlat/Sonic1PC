#ifndef _FAN_H
#define _FAN_H

#include "Object.h"

typedef struct {
    uint8_t pad0[8];   // 0x28-0x2F (0x28 = subtype: bit0 = blows backwards, bit1 = always on)
    int16_t time;        // 0x30 -- time until next on/off switch
    uint8_t fan_switch;    // 0x32 -- 0 = currently blowing, nonzero = currently paused
} Scratch_Fan;

void Obj_Fan(Object *obj);

#endif //_FAN_H
