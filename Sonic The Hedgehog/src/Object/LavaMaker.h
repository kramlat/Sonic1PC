#ifndef _LAVAMAKER_H
#define _LAVAMAKER_H

#include "Sst.h"

typedef struct {
    uint8_t fire_interval; // firing interval in frames (multiples of 30) -- real
                           // hardware repurposes the standard obDelayAni field for
                           // this; kept in our own scratch instead, since this
                           // project doesn't expose that field. The actual
                           // countdown itself reuses obj->frame_time (obTimeFrame).
} Own_LavaMaker;

void Obj_LavaMaker(Sst *self);

#endif //_LAVAMAKER_H
