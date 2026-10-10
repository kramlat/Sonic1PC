#ifndef _HTZBOSS_H
#define _HTZBOSS_H

#include "Object.h"

// Hill Top's boss of the alpha (object 52, which also makes its own parts): Robotnik's ship sits at one side of the arena and rises, opens its cannon and shoots a jet of flame across the arena, sinks again and
// throws two balls of lava that burn where they land, and comes up at the side that Sonic is not. Defeated, it blows up in smoke and the act ends.
// The parts are the same object with another routine: 4 the jet of flame, 6 the lava balls, 8 the smoke.
#define ObjId_HTZBoss 0x52

void Obj_HTZBoss(Object *obj);

// While the cannon is open the alpha's touch code gives the boss a second hitbox, the cannon's, sticking out to the side by how far it has opened (Touch_Boss). Returns true with the box's centre and half width
// (its half height is 4) when it is there; `hit` is the boss.
bool HTZBoss_CannonBox(const Object *hit, int16_t *centre_x, int16_t *centre_y, int16_t *half_width);

#endif //_HTZBOSS_H
