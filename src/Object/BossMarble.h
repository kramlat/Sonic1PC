#ifndef _BOSSMARBLE_H
#define _BOSSMARBLE_H

#include "Object.h"

// Object 73 - Eggman (MZ boss, "Egg Scorcher"). One physical object doubles
// as both the "controller" (boss_x/boss_y, generic timer, hit-flash, lava
// timer) and the visible ship (routine 2) -- it spawns 3 more objects of
// this same type for the face (routine 4), thruster flame (routine 6), and
// a static decorative tube (routine 8), which read the controller's state
// back via parent_index. See BossGreenHill.h for the equivalent GHZ layout
// this one directly parallels.
typedef struct {
    uint8_t pad0[8];        // 0x28-0x2F
    dword_s boss_x;         // 0x30-0x33 -- ship/controller only: fixed-point base X (see obBossX)
    uint8_t parent_index;   // 0x34 -- face/flame/tube only: index into objects[] for the ship/controller
    uint8_t lava_timer;     // real ASM aliases this onto the same byte as
                             // parent_index's pointer (BossMarble_LavaTimer);
                             // kept separate here since the ship/main
                             // instance never needs its own parent_index
    uint8_t pad1[2];        // 0x36-0x37
    dword_s boss_y;         // 0x38-0x3B -- ship/controller only: fixed-point base Y (see obBossY)
    int16_t generic_timer;  // 0x3C-0x3D -- ship/controller only
    uint8_t flash;          // 0x3E -- ship/controller only: remaining hit-flash frames
    uint8_t sine_counter;   // 0x3F -- ship/controller only: bobbing motion phase
} Scratch_BossMarble;

void Obj_BossMarble(Object *obj);

#endif //_BOSSMARBLE_H
