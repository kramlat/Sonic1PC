#ifndef _BOSSBLOCK_H
#define _BOSSBLOCK_H

#include "Sst.h"

// Object 76 - blocks that Eggman picks up (SYZ boss). 10 are spawned along
// the arena floor by the first block's own Main routine (mirroring
// BossSpringYard_Main's own self-multiplying spawn loop); each tracks its
// own column index (subtype) and the command byte the boss controller
// writes to steer it (child_cmd).
typedef struct {
    int8_t child_cmd;     // 0x29 -- command from the boss controller: 0 = normal/solid, -1 = grabbed, 0xA = breaking (real ASM's BossSpringYard_ChildCmd)
    uint8_t parent_index; // 0x34 -- index into sst[] for the boss controller (only meaningful while grabbed -- real ASM's BossSpringYard_ParentObj)
} Own_BossBlock;

void Obj_BossBlock(Sst *self);

#endif //_BOSSBLOCK_H
