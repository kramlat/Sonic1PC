#ifndef _SMASHBLOCK_H
#define _SMASHBLOCK_H

#include "Sst.h"


typedef struct {
    uint8_t sonic_anim; // 0x32 -- backup of Sonic's own animation ID right before SolidObject can change it
    uint16_t combo;      // 0x34 -- backup of the combo-score chain right before landing can change it
} Own_SmashBlock;

void Obj_SmashBlock(Sst *self);

#endif //_SMASHBLOCK_H
