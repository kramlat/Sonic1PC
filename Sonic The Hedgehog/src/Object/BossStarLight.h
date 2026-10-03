#ifndef _BOSSSTARLIGHT_H
#define _BOSSSTARLIGHT_H

#include "Object.h"

// Object 7A - Eggman (SLZ boss). The ship doubles as the controller and
// spawns 3 more of itself: face (routine 4), flame (6) and the wide pipe (8).
// He paces back and forth over the three boss-fight seesaws and drops
// spike balls onto them for Sonic to launch into his face.
typedef struct {
    int8_t pick;             // 0x28 -- ship only: seesaw list index chosen for the next drop (-1 = none) (obSubtype)
    uint8_t seesaw_count;    // 0x29 -- ship only: seesaws found in the list
    uint8_t seesaws[3];      // 0x2A-0x2C -- ship only: objects[] indexes of the boss-fight seesaws
    uint8_t pad0[3];         // 0x2D-0x2F
    dword_s boss_x;          // 0x30-0x33 -- ship only: fixed-point base X (obBossX)
    uint8_t parent_index;    // 0x34 -- face/flame/pipe only: index of the ship
    uint8_t pad1[3];         // 0x35-0x37
    dword_s boss_y;          // 0x38-0x3B -- ship only: fixed-point base Y (obBossY)
    uint8_t generic_timer;   // 0x3C -- ship only
    uint8_t pad2;            // 0x3D
    uint8_t flash;           // 0x3E -- ship only: remaining hit-flash frames (obBossFlash)
    uint8_t sine_counter;    // 0x3F -- ship only: bobbing phase
} Scratch_BossStarLight;

// Object 7B - the exploding spike balls (and their fragments). Offsets
// 0x30/0x34/0x3A/0x3C deliberately match Scratch_Seesaw.
typedef struct {
    uint16_t timer;          // 0x28 -- obSubtype used as a countdown
    uint8_t delay;           // 0x2A -- flicker frame delay
    uint8_t time_frame;      // 0x2B -- flicker frame timer
    uint8_t pad0[4];         // 0x2C-0x2F
    int16_t base_x;          // 0x30 -- seesaw X (obBossX); fragments: their own X
    uint8_t pad1[2];         // 0x32-0x33
    int16_t seesaw_y;        // 0x34 -- seesaw Y; fragments: their own Y
    uint8_t pad2[2];         // 0x36-0x37
    uint8_t pad3[2];         // 0x38-0x39
    int8_t side;             // 0x3A -- 0 = right of the seesaw, 2 = left
    uint8_t pad4;            // 0x3B
    uint8_t seesaw_index;    // 0x3C -- objects[] index of the linked seesaw
} Scratch_BossSpikeball;

void Obj_BossStarLight(Object *obj);
void Obj_BossSpikeball(Object *obj);

#endif //_BOSSSTARLIGHT_H
