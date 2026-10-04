#ifndef _TAILS_H
#define _TAILS_H

#include "Object.h"

#define TAILS_SLOT 0x1C // Tails' fixed object slot (the sidekick)

// Tails, for the code that has to treat him like Sonic (the solid objects touch both)
#define TAILS_OBJ (&objects[TAILS_SLOT])
void Tails_ResetOnFloor(Object *obj);
void KillTails(Object *obj);
int32_t Tails_Hurt(Object *obj, Object *src); // (hurts Tails as an object hurts Sonic: HurtSonic in Sonic.c)

#endif //_TAILS_H
