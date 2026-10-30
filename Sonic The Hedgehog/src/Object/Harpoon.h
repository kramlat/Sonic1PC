#ifndef _HARPOON_H
#define _HARPOON_H

#include "Sst.h"

typedef struct {
    int16_t time;    // 0x30 -- time to wait between extending/retracting
} Own_Harpoon;

void Obj_Harpoon(Sst *self);

#endif //_HARPOON_H
