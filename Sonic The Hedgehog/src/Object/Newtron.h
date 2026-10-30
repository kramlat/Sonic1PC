#ifndef _NEWTRON_H
#define _NEWTRON_H

#include "Sst.h"


void Obj_Newtron(Sst *self);

// Newtron object
typedef struct {
    uint8_t fired; // 0x32
} Own_Newtron;

#endif //_NEWTRON_H
