#ifndef _SIGNPOST_H
#define _SIGNPOST_H

#include "Sst.h"


extern const uint8_t Mappings_RingREV01[]; // From Ring.c

typedef struct {
    int16_t spin_time; // 0x30
    int16_t sparkle_time; // 0x32
    uint8_t sparkle_id; // 0x34
} Own_Signpost;

void Obj_Signpost(Sst *self);

#endif //_SIGNPOST_H
