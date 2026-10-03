#ifndef _BURROBOT_H
#define _BURROBOT_H

#include "Object.h"

typedef struct {
    uint8_t pad0[8];    // 0x28-0x2F
    int16_t timedelay;  // 0x30 -- timer used for waiting before turning around, or automatic action changes
    bool checktype;     // 0x32 -- (while moving) alternate between checking ledges ahead or aligning to floor
} Scratch_Burrobot;

void Obj_Burrobot(Object *obj);

#endif //_BURROBOT_H
