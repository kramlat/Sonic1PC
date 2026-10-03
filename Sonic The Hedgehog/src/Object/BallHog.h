#ifndef _BALLHOG_H
#define _BALLHOG_H

#include "Object.h"

typedef struct {
    uint8_t pad0[10]; // 0x28-0x31 (0x28 = subtype, explosion timer in seconds for spawned cannonballs)
    uint8_t launched;   // 0x32 -- set if a cannonball has already been launched this animation cycle
} Scratch_BallHog;

void Obj_BallHog(Object *obj);

#endif //_BALLHOG_H
