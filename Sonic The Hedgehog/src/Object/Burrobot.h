#ifndef _BURROBOT_H
#define _BURROBOT_H

#include "Sst.h"

typedef struct {
    int16_t timedelay;  // 0x30 -- timer used for waiting before turning around, or automatic action changes
    bool checktype;     // 0x32 -- (while moving) alternate between checking ledges ahead or aligning to floor
} Own_Burrobot;

void Obj_Burrobot(Sst *self);

#endif //_BURROBOT_H
