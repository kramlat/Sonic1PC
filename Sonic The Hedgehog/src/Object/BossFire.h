#ifndef _BOSSFIRE_H
#define _BOSSFIRE_H

#include "Sst.h"

// Object 74 - lava that Eggman drops (MZ boss). Doubles as the falling
// fireball itself (subtype != 0) and the "temp fire" floor warning marker
// it leaves behind as it spreads along the lava pool (subtype == 0).
typedef struct {
    uint8_t generic_timer;  // 0x29 -- BossFire_GenericTimer
    int16_t boss_x;         // 0x30-0x31 -- BossFire_BossX (plain word; unlike
                             // BossMarble's, this is never fed through
                             // BossMove, so it's not fixed-point)
    int16_t spread_x;       // 0x32-0x33 -- BossFire_SpreadX
    int16_t boss_y;         // 0x38-0x39 -- BossFire_BossY
} Own_BossFire;

void Obj_BossFire(Sst *self);

#endif //_BOSSFIRE_H
