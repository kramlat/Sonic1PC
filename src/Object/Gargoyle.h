#ifndef _GARGOYLE_H
#define _GARGOYLE_H

#include "Object.h"

typedef struct {
    uint8_t pad0[8];  // 0x28-0x2F
    uint8_t spit_rate; // real hardware stores this in the standard obDelayAni
                        // field, which this port's Object struct doesn't have
                        // -- kept in scratch instead (frame_time.b is still
                        // the countdown, matching real obTimeFrame)
} Scratch_Gargoyle;

void Obj_Gargoyle(Object *obj);

#endif //_GARGOYLE_H
