#ifndef _SPIKES_H
#define _SPIKES_H

#include "Sst.h"

// The object's own words (Sst::own)
typedef struct {
    int16_t orig_x;
    int16_t orig_y;
    word_u move;
    uint16_t dir;
    uint16_t timer;
} Own_Spikes;

void Obj_Spikes(Sst *self);

#endif //_SPIKES_H
