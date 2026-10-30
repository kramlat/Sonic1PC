#ifndef _SPRING_H
#define _SPRING_H

#include "Sst.h"

// The object's own words (Sst::own)
typedef struct {
    int16_t power;
} Own_Spring;

void Obj_Spring(Sst *self);

#endif //_SPRING_H
