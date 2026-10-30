#ifndef _BALLHOG_H
#define _BALLHOG_H

#include "Sst.h"

typedef struct {
    uint8_t launched;   // 0x32 -- set if a cannonball has already been launched this animation cycle
} Own_BallHog;

void Obj_BallHog(Sst *self);

#endif //_BALLHOG_H
