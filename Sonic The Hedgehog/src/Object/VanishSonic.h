#ifndef _VANISHSONIC_H
#define _VANISHSONIC_H

#include "Sst.h"

// Object 4A - unused and unfinished Special Stage entry effect from the
// beta. Nothing in the final game ever spawns this, but it's fully
// self-contained and harmless to have available (e.g. for hacks/mods that
// want a "vanish into a warp" effect).
typedef struct {
    int16_t time;       // 0x30 -- time for Sonic's disappearance (2 seconds)
} Own_VanishSonic;

void Obj_VanishSonic(Sst *self);

#endif //_VANISHSONIC_H
