#ifndef _COCONUTS_H
#define _COCONUTS_H

#include "Object.h"

// Emerald Hill's Coconuts (the alpha's object 9D): a monkey that climbs up and down a palm trunk and throws coconuts at Sonic, and its weapon (object 98: the alpha's one object for what its badniks throw; only the
// coconut is ported so far).
#define ObjId_Coconuts    0x9D
#define ObjId_EnemyWeapon 0x98

void Obj_Coconuts(Object *obj);
void Obj_EnemyWeapon(Object *obj);

#endif //_COCONUTS_H
