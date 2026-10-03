#ifndef _EXPLOSION_H
#define _EXPLOSION_H

#include "Object.h"

// Explosion assets
#include "Resource/Mappings/Explosion.h"
// Real hardware's Map_ExplodeBomb shares 3 of its 5 frames with
// Map_ExplodeItem via backwards (negative) mappingsTable offsets -- not
// representable here since this engine's frame lookup treats mapping
// offsets as unsigned. Mappings_ExplodeBomb is therefore its own
// standalone resource with those shared frames duplicated in rather than
// referenced.
#include "Resource/Mappings/ExplodeBomb.h"

void Obj_Explosion_Animal(Object *obj);
void Obj_Explosion_Construct(Object *obj);
void Obj_Explosion_Animate(Object *obj);
void Obj_Explosion(Object *obj);

// Object 3F - fiery explosion from a destroyed boss, Walking Bomb badnik,
// or Ball Hog cannonball. Shares its animate/delete tail with Object 27
// above (Obj_Explosion_Animate) but uses its own mapping table and sound.
void Obj_ExplosionBomb_Construct(Object *obj);
void Obj_ExplosionBomb(Object *obj);

#endif
