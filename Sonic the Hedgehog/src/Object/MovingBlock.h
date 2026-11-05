#ifndef _MOVINGBLOCK_H
#define _MOVINGBLOCK_H

#include "Sst.h"


typedef struct {
    int16_t orig_x;        // 0x30
    int16_t orig_y;        // 0x32
    int16_t slide_wait;     // 0x34 -- subtype 9/A only: delay before the red sliding floor moves back
    int16_t slide_goback;   // 0x36 -- subtype 9/A only: set while the red sliding floor is moving back
} Own_MovingBlock;

void Obj_MovingBlock(Sst *self);

#endif //_MOVINGBLOCK_H
