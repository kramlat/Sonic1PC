#ifndef _DUSTSPLASH_H
#define _DUSTSPLASH_H

#include "Object.h"

// Object 08 as the alpha has it: the dust of the spin dash and of the skid, and the splash of the water, an object for each player. Sonic's is where Sonic 1's level start puts it (slot 0x1B), Tails' in DUST_TAILS_SLOT
// (the level's own objects make it).
#define DUST_TAILS_SLOT 0x15

// What it shows (its animation)
#define DUST_NULL   0
#define DUST_SPLASH 1
#define DUST_DASH   2 // (the spin dash's: where the player is when it starts)
#define DUST_SKID   3

void Obj_DustSplash(Object *obj);

// A player's dust is sent to show something (the animation starts over; the splash is where he is, at the water's height)
void DustSplash_Show(Object *dust, uint8_t what);

// The object the level's own objects make for Tails (a spin dash's dust and a splash of his own)
void DustSplash_MakeTails(void);

#endif //_DUSTSPLASH_H
