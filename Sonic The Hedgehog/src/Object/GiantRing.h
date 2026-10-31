#ifndef _GIANTRING_H
#define _GIANTRING_H

#include "Sst.h"


typedef struct {
    Sst *parent; // parent Giant Ring object -- set to routine 6 (delete) once the flash reaches its 3rd frame
} Own_RingFlash;

void Obj_GiantRing(Sst *self);
void Obj_RingFlash(Sst *self);

#endif //_GIANTRING_H
