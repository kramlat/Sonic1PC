#ifndef _MOVINGBLOCK_H
#define _MOVINGBLOCK_H

#include "Object.h"

// Moving block assets
#include "Resource/Mappings/MovingBlocks.h"
#include "Resource/Mappings/LZMovingBlocks.h"

typedef struct {
    uint8_t pad0[8];      // 0x28-0x2F
    int16_t orig_x;        // 0x30
    int16_t orig_y;        // 0x32
    int16_t slide_wait;     // 0x34 -- subtype 9/A only: delay before the red sliding floor moves back
    int16_t slide_goback;   // 0x36 -- subtype 9/A only: set while the red sliding floor is moving back
} Scratch_MovingBlock;

void Obj_MovingBlock(Object *obj);

#endif //_MOVINGBLOCK_H
