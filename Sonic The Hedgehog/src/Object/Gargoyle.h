#ifndef _GARGOYLE_H
#define _GARGOYLE_H

#include "Sst.h"

typedef struct {
    uint8_t spit_rate; // real hardware stores this in the standard obDelayAni
                        // field, which this port's Object struct doesn't have
                        // -- kept in scratch instead (frame_time.b is still
                        // the countdown, matching real obTimeFrame)
} Own_Gargoyle;

void Obj_Gargoyle(Sst *self);

#endif //_GARGOYLE_H
