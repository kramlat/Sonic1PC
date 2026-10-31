#ifndef _HTZOBJECTS_H
#define _HTZOBJECTS_H

#include "Sst.h"

void Obj_HTZSeesaw(Sst *self);
void Obj_HTZLift(Sst *self);
void Obj_HTZBreakFloor(Sst *self);
void Obj_HTZLavaBox(Sst *self);
void Obj_HTZFireball(Sst *self);

// A lava ball of the alpha's Hill Top boss has landed: it becomes a fireball burning where it landed
void HTZFireball_BossBallLands(Sst *self);

#endif //_HTZOBJECTS_H
