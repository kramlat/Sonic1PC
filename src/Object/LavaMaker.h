#ifndef _LAVAMAKER_H
#define _LAVAMAKER_H

#include "Object.h"

typedef struct {
    uint8_t pad0;         // 0x28 (subtype, read directly via obj->scratch.u8[0])
    uint8_t fire_interval; // firing interval in frames (multiples of 30) -- real
                           // hardware repurposes the standard obDelayAni field for
                           // this; kept in our own scratch instead, since this
                           // project doesn't expose that field. The actual
                           // countdown itself reuses obj->frame_time (obTimeFrame).
} Scratch_LavaMaker;

void Obj_LavaMaker(Object *obj);

#endif //_LAVAMAKER_H
