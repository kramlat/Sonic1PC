#ifndef _BOSSGREENHILL_H
#define _BOSSGREENHILL_H

#include "Sst.h"

// Object 3D - Eggman (GHZ boss)
typedef struct {
    dword_s boss_x;          // 0x30-0x33 -- ship/controller only: fixed-point base X (see obBossX)
    uint8_t parent_index;       // 0x34 -- face/flame only: index into sst[] for the ship/controller
    dword_s boss_y;                   // 0x38-0x3B -- ship/controller only: fixed-point base Y (see obBossY)
    int16_t generic_timer;               // 0x3C-0x3D -- ship/controller only
    uint8_t flash;                          // 0x3E -- ship/controller only: remaining hit-flash frames
    uint8_t sine_counter;                      // 0x3F -- ship/controller only: bobbing motion phase
} Own_BossGreenHill;

void Obj_BossGreenHill(Sst *self);

#endif //_BOSSGREENHILL_H
