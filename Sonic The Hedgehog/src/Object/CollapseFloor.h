#ifndef _COLLAPSEFLOOR_H
#define _COLLAPSEFLOOR_H

#include "Sst.h"


typedef struct {
    uint8_t timedelay;  // 0x38 -- same shared layout as Own_CollapseLedge (see its own comment)
    uint8_t flag;       // 0x3A
} Own_CollapseFloor;

void Obj_CollapseFloor(Sst *self);

#endif //_COLLAPSEFLOOR_H
