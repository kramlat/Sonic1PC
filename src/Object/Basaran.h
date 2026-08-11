#ifndef _BASARAN_H
#define _BASARAN_H

#include "Object.h"

typedef struct {
    uint8_t pad0[14]; // 0x28-0x35 (unused -- this object doesn't read a subtype)
    int16_t sonic_y;    // 0x36 -- Sonic's own Y-position at the moment this Basaran started dropping from the ceiling
} Scratch_Basaran;

void Obj_Basaran(Object *obj);

#endif //_BASARAN_H
