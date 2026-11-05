#ifndef _ELECTROCUTER_H
#define _ELECTROCUTER_H

#include "Sst.h"

typedef struct {
    uint16_t freq;      // 0x34 -- zapping frequency as an ANDable value
} Own_Electrocuter;

void Obj_Electrocuter(Sst *self);

#endif //_ELECTROCUTER_H
