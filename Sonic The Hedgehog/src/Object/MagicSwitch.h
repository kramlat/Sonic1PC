#ifndef _MAGICSWITCH_H
#define _MAGICSWITCH_H

#include "Object.h"

// Object 1D - switch that activates when Sonic touches it. Not used
// anywhere in the game -- real hardware's own comment notes its art tile
// is $000 ("probably broken anyway", since no PLC was ever set up to load
// dedicated art for it, so its mapping tiles reference whatever happens to
// already be in VRAM). Kept faithfully as-is for modders who want a usable
// touch-switch.
typedef struct {
    uint8_t pad0[8]; // 0x28-0x2F
    int16_t orig_y;     // 0x30
} Scratch_MagicSwitch;

void Obj_MagicSwitch(Object *obj);

#endif //_MAGICSWITCH_H
