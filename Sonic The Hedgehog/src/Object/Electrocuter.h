#ifndef _ELECTROCUTER_H
#define _ELECTROCUTER_H

#include "Object.h"

typedef struct {
    uint8_t pad0[12]; // 0x28-0x33 (0x28 = subtype)
    uint16_t freq;      // 0x34 -- zapping frequency as an ANDable value
} Scratch_Electrocuter;

void Obj_Electrocuter(Object *obj);

#endif //_ELECTROCUTER_H
