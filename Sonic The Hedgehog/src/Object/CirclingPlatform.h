#ifndef _CIRCLINGPLATFORM_H
#define _CIRCLINGPLATFORM_H

#include "Sst.h"

typedef struct {
    int16_t orig_y;    // 0x30
    int16_t orig_x;     // 0x32
} Own_CirclingPlatform;

void Obj_CirclingPlatform(Sst *self);

#endif //_CIRCLINGPLATFORM_H
