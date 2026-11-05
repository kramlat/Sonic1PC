#ifndef _BASARAN_H
#define _BASARAN_H

#include "Sst.h"

typedef struct {
    int16_t sonic_y;    // 0x36 -- Sonic's own Y-position at the moment this Basaran started dropping from the ceiling
} Own_Basaran;

void Obj_Basaran(Sst *self);

#endif //_BASARAN_H
