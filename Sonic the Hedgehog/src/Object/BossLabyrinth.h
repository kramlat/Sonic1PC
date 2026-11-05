#ifndef _BOSSLABYRINTH_H
#define _BOSSLABYRINTH_H

#include "Sst.h"

// Object 77 - Eggman (LZ boss). No attack of his own: the ship rises up a
// vertical shaft lined with spikes, gargoyle fireballs and harpoons while the
// water rises underneath (DynWater_LZ3's routine 4), and Sonic has to keep
// up. Same controller/ship-doubles-as-parent layout as BossSpringYard.h,
// spawning 2 more of itself for the face (routine 4) and thruster flame
// (routine 6).
typedef struct {
    dword_s boss_x;          // 0x30-0x33 -- ship/controller only: fixed-point base X (obBossX)
    uint8_t parent_index;    // 0x34 -- face/flame only: index into sst[] for the ship/controller
    dword_s boss_y;          // 0x38-0x3B -- ship/controller only: fixed-point base Y (obBossY)
    uint8_t generic_timer;   // 0x3C -- ship only: short pause before escaping
    uint8_t early_defeat;    // 0x3D -- ship only: $FF once beaten before reaching the top
    uint8_t flash;           // 0x3E -- ship only: remaining hit-flash frames (obBossFlash)
    uint8_t sine_counter;    // 0x3F -- ship only: side-to-side sway phase during the climb
} Own_BossLabyrinth;

void Obj_BossLabyrinth(Sst *self);

#endif //_BOSSLABYRINTH_H
