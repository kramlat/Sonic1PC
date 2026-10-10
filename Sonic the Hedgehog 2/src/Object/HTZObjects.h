#ifndef _HTZOBJECTS_H
#define _HTZOBJECTS_H

#include "Object.h"

void Obj_HTZSeesaw(Object *obj);
void Obj_HTZLift(Object *obj);
void Obj_HTZBreakFloor(Object *obj);
void Obj_HTZLavaBox(Object *obj);
void Obj_HTZFireball(Object *obj);

// A lava ball of the alpha's Hill Top boss has landed: it becomes a fireball burning where it landed
void HTZFireball_BossBallLands(Object *obj);

#endif //_HTZOBJECTS_H
