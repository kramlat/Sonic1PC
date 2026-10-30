#ifndef _EXPLOSION_H
#define _EXPLOSION_H

#include "EngineObject.h"
#include "Sst.h"

// Object 27 - the explosion of a destroyed badnik (it lets an animal out), and object 3F - the fiery one of a destroyed boss, a Walking Bomb or a Ball Hog cannonball. The objects on the status table.
void Obj_Explosion(Sst *self);
void Obj_ExplosionBomb(Sst *self);

// What the old objects that turn into an explosion by setting their `type` run (an `Object` of these ids): it makes the explosion on the status table where the object was, with its routine and the badnik's
// score (scratch word 0xB), then stays in the object table, invisible, for as long as the explosion lasts: the objects that were made by what exploded check `parent->type == ObjId_Explosion` to know it did
// (Caterkiller, Buzz Bomber). Goes when the last of those has moved over and the sites that set the type make the explosion directly.
void Obj_ExplosionObject(Object *obj);
void Obj_ExplosionBombObject(Object *obj);

#endif
