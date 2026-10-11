#ifndef _CNZBOSS_H
#define _CNZBOSS_H

#include "Object.h"

// Casino Night's boss of the alpha (object 51, which also makes the ball it lets fall: the same object with another routine, 4): Robotnik's ship, with a catcher at each side, goes round the corners of the arena,
// opens its catchers, and lets a ball fall from one (the ball bounces about the arena, no more than that: nothing hurts or is hurt by it). Defeated, it blows up, the catchers fall, it leaves, and the act ends.
#define ObjId_CNZBoss 0x51

void Obj_CNZBoss(Object *obj);

// DynResize_CNz of act 2 (nothing in act 1): the camera stops at the arena and the boss comes
void CNZBoss_Events(void);

#endif //_CNZBOSS_H
