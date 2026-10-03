#ifndef _COLLAPSEFLOOR_H
#define _COLLAPSEFLOOR_H

#include "Object.h"

// Collapsing floor assets
#include "Resource/Mappings/CollapsingFloors.h"

typedef struct {
    uint8_t pad0[0x10]; // 0x28-0x37
    uint8_t timedelay;  // 0x38 -- same shared layout as Scratch_CollapseLedge (see its own comment)
    uint8_t pad1;       // 0x39
    uint8_t flag;       // 0x3A
} Scratch_CollapseFloor;

void Obj_CollapseFloor(Object *obj);

#endif //_COLLAPSEFLOOR_H
