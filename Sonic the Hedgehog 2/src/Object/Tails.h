#ifndef _TAILS_H
#define _TAILS_H

#include "Sst.h"

#define TAILS_SLOT 0x1C // Tails' fixed object slot (the sidekick)

// Tails, for the code that has to treat him like Sonic (the solid objects touch both)
#define TAILS_OBJ SST_PLAYER_2 // Tails: the second character (Player_2)
void Tails_ResetOnFloor(Sst *self);
void KillTails(Sst *self);
int32_t Tails_Hurt(Sst *self, Object *src); // (hurts Tails as an object hurts Sonic: HurtSonic in Sonic.c)

#endif //_TAILS_H
