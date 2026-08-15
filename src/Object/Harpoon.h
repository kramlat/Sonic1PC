#ifndef _HARPOON_H
#define _HARPOON_H

#include "Object.h"

typedef struct {
    uint8_t pad0[8]; // 0x28-0x2F
    int16_t time;    // 0x30 -- time to wait between extending/retracting
} Scratch_Harpoon;

void Obj_Harpoon(Object *obj);

#endif //_HARPOON_H
