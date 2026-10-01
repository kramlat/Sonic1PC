#ifndef _SCRAPEGGMAN_H
#define _SCRAPEGGMAN_H

#include "Object.h"

// Object 82 - Eggman at the end of SBZ2 (cutscene): he waits by a floor switch, leaps onto it when Sonic comes
// near, and the false floor (object 83) crumbles away under Sonic, dropping him out of the level and on to SBZ3.
typedef struct {
    uint16_t cmd;          // 0x28 -- "SW"/"GO" command word (obSubtype); Eggman sends SW, then GO to the floor
    uint8_t pad0[6];       // 0x2A-0x2F
    uint8_t pad1[4];       // 0x30-0x33
    uint8_t parent_index;  // 0x34 -- the switch only: index of Eggman
    uint8_t pad2[7];       // 0x35-0x3B
    int16_t timer;         // 0x3C -- Eggman only: frames before the next step
} Scratch_ScrapEggman;

// Object 83 - the false floor: a manager object (routine 2/4) that owns 8 block objects (routine 8), which break
// away one by one into 4 fragments each (routine $A).
typedef struct {
    uint16_t cmd;          // 0x28 -- manager: waits for "GO"; blocks: "GO" = break up now
    uint8_t pad0[6];       // 0x2A-0x2F
    uint8_t blocks[8];     // 0x30-0x37 -- manager only: objects[] index of each block
    uint8_t time_frame;    // 0x38 -- manager only: break pacing counter (obTimeFrame)
    uint8_t break_count;   // 0x39 -- manager only: blocks broken so far (obFrame)
} Scratch_FalseFloor;

void Obj_ScrapEggman(Object *obj);
void Obj_FalseFloor(Object *obj);

#endif //_SCRAPEGGMAN_H
