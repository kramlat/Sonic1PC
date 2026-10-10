#ifndef _MTZBOSS_H
#define _MTZBOSS_H

#include "Object.h"

// Metropolis's boss of the alpha, which is not finished in it (and unreachable: the level's events for it start with an `rts`): Robotnik's ship (objects 54 and 55, the same code) with seven balls that orbit it
// (object 53, which the ship makes: the first of them is the object that the ship makes itself). Hit, the ship lets the balls go; they fall, and a small Robotnik walks out of each.
#define ObjId_MTZBoss 0x54
#define ObjId_MTZBoss2 0x55
#define ObjId_MTZBossBall 0x53

void Obj_MTZBoss(Object *obj);
void Obj_MTZBossBall(Object *obj);

#endif //_MTZBOSS_H
