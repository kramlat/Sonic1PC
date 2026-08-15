#ifndef _BOSSGREENHILL_H
#define _BOSSGREENHILL_H

#include "Object.h"

// Object 3D - Eggman (GHZ boss)
typedef struct {
    uint8_t pad0[8];      // 0x28-0x2F
    dword_s boss_x;          // 0x30-0x33 -- ship/controller only: fixed-point base X (see obBossX)
    uint8_t parent_index;       // 0x34 -- face/flame only: index into objects[] for the ship/controller
    uint8_t pad1[3];               // 0x35-0x37
    dword_s boss_y;                   // 0x38-0x3B -- ship/controller only: fixed-point base Y (see obBossY)
    int16_t generic_timer;               // 0x3C-0x3D -- ship/controller only
    uint8_t flash;                          // 0x3E -- ship/controller only: remaining hit-flash frames
    uint8_t sine_counter;                      // 0x3F -- ship/controller only: bobbing motion phase
} Scratch_BossGreenHill;

void Obj_BossGreenHill(Object *obj);

#endif //_BOSSGREENHILL_H
