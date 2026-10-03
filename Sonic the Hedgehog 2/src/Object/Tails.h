#ifndef _TAILS_H
#define _TAILS_H

#include "Object.h"

#define TAILS_SLOT 0x1C // Tails' fixed object slot (the sidekick)

// Tails, for the code that has to treat him like Sonic (the solid objects touch both)
#define TAILS_OBJ (&objects[TAILS_SLOT])
void Tails_ResetOnFloor(Object *obj);
void KillTails(Object *obj);

#endif //_TAILS_H
