#ifndef _MAGICSWITCH_H
#define _MAGICSWITCH_H

#include "Sst.h"

// Object 1D - switch that activates when Sonic touches it. Not used
// anywhere in the game -- real hardware's own comment notes its art tile
// is $000 ("probably broken anyway", since no PLC was ever set up to load
// dedicated art for it, so its mapping tiles reference whatever happens to
// already be in VRAM). Kept faithfully as-is for modders who want a usable
// touch-switch.
typedef struct {
    int16_t orig_y;     // 0x30
} Own_MagicSwitch;

void Obj_MagicSwitch(Sst *self);

#endif //_MAGICSWITCH_H
