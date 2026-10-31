#ifndef _SPIKES_H
#define _SPIKES_H

#include "Sst.h"


typedef struct {
    int16_t orig_x; // 0x30
    int16_t orig_y; // 0x32
    word_u move; // 0x34
    uint16_t dir; // 0x36
    uint16_t timer; // 0x38
} Own_Spikes;

#endif //_SPIKES_H
