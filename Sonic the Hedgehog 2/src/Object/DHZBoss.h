#ifndef _DHZBOSS_H
#define _DHZBOSS_H

#include "Object.h"

// Dust Hill's boss of the alpha (object 57, which also makes the rocks that fall from the ceiling as it digs): Robotnik's ship, with a drill at each end, comes up out of the floor of the arena, drops down again
// at the other end of the arena and charges, back and forth, with the ceiling shaking rocks loose all the while. Defeated, it blows up and the drills fall off, and the ship leaves across the arena (the act ends
// when it has gone). The rocks are the same object with another routine (4).
#define ObjId_DHZBoss 0x57

void Obj_DHZBoss(Object *obj);

// DynResize_DHz of act 2: the arena at the end (the events of the alpha: nothing in act 1)
void DHZBoss_Events(void);

#endif //_DHZBOSS_H
