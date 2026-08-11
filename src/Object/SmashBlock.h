#ifndef _SMASHBLOCK_H
#define _SMASHBLOCK_H

#include "Object.h"

// Smashable green block assets
#include "Resource/Mappings/SmashableGreenBlock.h"

typedef struct {
    uint8_t pad0[10]; // 0x28-0x31
    uint8_t sonic_anim; // 0x32 -- backup of Sonic's own animation ID right before SolidObject can change it
    uint8_t pad1;       // 0x33
    uint16_t combo;      // 0x34 -- backup of the combo-score chain right before landing can change it
} Scratch_SmashBlock;

void Obj_SmashBlock(Object *obj);

#endif //_SMASHBLOCK_H
