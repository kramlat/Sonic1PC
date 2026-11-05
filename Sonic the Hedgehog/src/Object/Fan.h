#ifndef _FAN_H
#define _FAN_H

#include "Sst.h"

typedef struct {
    int16_t time;        // 0x30 -- time until next on/off switch
    uint8_t fan_switch;    // 0x32 -- 0 = currently blowing, nonzero = currently paused
} Own_Fan;

void Obj_Fan(Sst *self);

#endif //_FAN_H
