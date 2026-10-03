#ifndef _BOSSSPRINGYARD_H
#define _BOSSSPRINGYARD_H

#include "Object.h"

// Object 75 - Eggman (SYZ boss, "Egg Stinger"). One physical object doubles
// as both the "controller" (boss_x/boss_y, generic timer, hit-flash, sine
// bobbing) and the visible ship (routine 2) -- it spawns 3 more objects of
// this same type for the face (routine 4), thruster flame (routine 6), and
// the grabbing spike (routine 8), which read the controller's state back
// via parent_index. Directly parallels BossGreenHill.h/BossMarble.h's own
// layout, plus the block-grabbing attack's own state (child_cmd,
// block_index/has_block). See BossBlock.h for the separate object (76) the
// spike attack grabs and breaks.
typedef struct {
    uint8_t pad0;           // 0x28 (subtype -- ship-only attack sub-state, read directly via obj->scratch.u8[0])
    uint8_t spike_disabled;  // 0x29 -- ship only: spike hurt-collision disabled while a block is grabbed or breaking (real ASM's BossSpringYard_ChildCmd, aliased onto the same offset the block object's own child_cmd uses -- see BossBlock.h)

    // 0x2A -- ShipMove-only: whether Eggman has already dropped down to
    // attack during the current left/right sweep. Real ASM aliases this
    // onto generic_timer's own low byte (BossSpringYard_PhaseTimer) --
    // harmless there since generic_timer isn't meaningfully "in use"
    // outside the Attack sub-states, but that coincidence isn't worth
    // preserving in C; kept as its own byte here instead (borrowed from
    // otherwise-unused padding, not appended past the struct -- obj->scratch
    // is a fixed 24-byte union, see Object.h). (The one place real ASM reads
    // this flag's bits *while* generic_timer is actively being used --
    // BSYZ_Lift's shake-check -- reads generic_timer's own current low byte
    // directly in this port, not this field.)
    bool swept;
    uint8_t pad1[5];         // 0x2B-0x2F
    dword_s boss_x;          // 0x30-0x33 -- ship/controller only: fixed-point base X (see obBossX)
    uint8_t parent_index;    // 0x34 -- face/flame/spike only: index into objects[] for the ship/controller
                              //         (ship only: doubles as which 32px-wide block column Eggman is currently over -- real ASM's BossSpringYard_BlockIndex, same offset, different alias)
    uint8_t pad2;            // 0x35
    uint8_t block_index;     // 0x36 -- ship only: object-slot index of the currently grabbed/targeted block (real ASM's BossSpringYard_ObjPointer, a raw pointer there -- an index here, see has_block)
    bool has_block;          // 0x37 -- ship only: whether block_index above is currently valid
    dword_s boss_y;          // 0x38-0x3B -- ship/controller only: fixed-point base Y (see obBossY)
    int16_t generic_timer;   // 0x3C-0x3D -- ship/controller only
    uint8_t flash;           // 0x3E -- ship/controller only: remaining hit-flash frames
    uint8_t sine_counter;    // 0x3F -- ship/controller only: bobbing motion phase
} Scratch_BossSpringYard;

void Obj_BossSpringYard(Object *obj);

#endif //_BOSSSPRINGYARD_H
