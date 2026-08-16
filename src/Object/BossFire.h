#ifndef _BOSSFIRE_H
#define _BOSSFIRE_H

#include "Object.h"

// Object 74 - lava that Eggman drops (MZ boss). Doubles as the falling
// fireball itself (subtype != 0) and the "temp fire" floor warning marker
// it leaves behind as it spreads along the lava pool (subtype == 0).
typedef struct {
    uint8_t pad0;           // 0x28 (subtype, read directly via obj->scratch.u8[0])
    uint8_t generic_timer;  // 0x29 -- BossFire_GenericTimer
    uint8_t pad1[6];        // 0x2A-0x2F
    int16_t boss_x;         // 0x30-0x31 -- BossFire_BossX (plain word; unlike
                             // BossMarble's, this is never fed through
                             // BossMove, so it's not fixed-point)
    int16_t spread_x;       // 0x32-0x33 -- BossFire_SpreadX
    uint8_t pad2[4];        // 0x34-0x37
    int16_t boss_y;         // 0x38-0x39 -- BossFire_BossY
} Scratch_BossFire;

void Obj_BossFire(Object *obj);

#endif //_BOSSFIRE_H
