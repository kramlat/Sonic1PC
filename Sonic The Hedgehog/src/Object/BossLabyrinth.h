#ifndef _BOSSLABYRINTH_H
#define _BOSSLABYRINTH_H

#include "Object.h"

// Object 77 - Eggman (LZ boss). No attack of his own: the ship rises up a
// vertical shaft lined with spikes, gargoyle fireballs and harpoons while the
// water rises underneath (DynWater_LZ3's routine 4), and Sonic has to keep
// up. Same controller/ship-doubles-as-parent layout as BossSpringYard.h,
// spawning 2 more of itself for the face (routine 4) and thruster flame
// (routine 6).
typedef struct {
    uint8_t pad0[8];         // 0x28-0x2F
    dword_s boss_x;          // 0x30-0x33 -- ship/controller only: fixed-point base X (obBossX)
    uint8_t parent_index;    // 0x34 -- face/flame only: index into objects[] for the ship/controller
    uint8_t pad1[3];         // 0x35-0x37
    dword_s boss_y;          // 0x38-0x3B -- ship/controller only: fixed-point base Y (obBossY)
    uint8_t generic_timer;   // 0x3C -- ship only: short pause before escaping
    uint8_t early_defeat;    // 0x3D -- ship only: $FF once beaten before reaching the top
    uint8_t flash;           // 0x3E -- ship only: remaining hit-flash frames (obBossFlash)
    uint8_t sine_counter;    // 0x3F -- ship only: side-to-side sway phase during the climb
} Scratch_BossLabyrinth;

void Obj_BossLabyrinth(Object *obj);

#endif //_BOSSLABYRINTH_H
