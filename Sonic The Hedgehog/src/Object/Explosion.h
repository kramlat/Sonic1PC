#ifndef _EXPLOSION_H
#define _EXPLOSION_H

#include "EngineObject.h"
#include "Sst.h"

// Object 27 - the explosion of a destroyed badnik (it lets an animal out), and object 3F - the fiery one of a destroyed boss, a Walking Bomb or a Ball Hog cannonball. The objects on the status table.
void Obj_Explosion(Sst *self);
void Obj_ExplosionBomb(Sst *self);

#endif
