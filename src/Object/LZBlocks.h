#ifndef _LZBLOCKS_H
#define _LZBLOCKS_H

#include "Object.h"

typedef struct {
    uint8_t pad0[8];   // 0x28-0x2F
    int16_t orig_y;    // 0x30 -- original y-axis position
    uint8_t pad1[2];   // 0x32-0x33
    int16_t orig_x;    // 0x34 -- original x-axis position
    int16_t time;      // 0x36 -- time delay for block movement
    bool untouched;    // 0x38 -- flag block as untouched
    uint8_t pad2[5];   // 0x39-0x3D
    uint8_t nudge;     // 0x3E -- nudge Y-offset while Sonic is standing on block
    int8_t touchtype;  // 0x3F -- Sonic's touch response from SolidObject (0, +1, -1)
} Scratch_LBlock;

void Obj_LabyrinthBlock(Object *obj);

#endif //_LZBLOCKS_H
