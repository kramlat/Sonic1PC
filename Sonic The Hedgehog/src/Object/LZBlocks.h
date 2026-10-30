#ifndef _LZBLOCKS_H
#define _LZBLOCKS_H

#include "Sst.h"

typedef struct {
    int16_t orig_y;    // 0x30 -- original y-axis position
    int16_t orig_x;    // 0x34 -- original x-axis position
    int16_t time;      // 0x36 -- time delay for block movement
    bool untouched;    // 0x38 -- flag block as untouched
    uint8_t nudge;     // 0x3E -- nudge Y-offset while Sonic is standing on block
    int8_t touchtype;  // 0x3F -- Sonic's touch response from SolidObject (0, +1, -1)
} Own_LBlock;

void Obj_LabyrinthBlock(Sst *self);

#endif //_LZBLOCKS_H
