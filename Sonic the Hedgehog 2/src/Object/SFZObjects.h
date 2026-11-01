#ifndef _SFZOBJECTS_H
#define _SFZOBJECTS_H

#include "Sst.h"

// Sky Fortress's own objects of the pre-beta (Wing Fortress in the final game; its object ids are $B0 and up)

// $BA: the wheels of a conveyor belt (a picture of 32 by 32 pixels that stays: in the pre-beta nothing turns it, and the level's dynamic events are what is to move it when it is used)
void Obj_SFZConveyorWheels(Sst *self);

// $D9: something to hang from (an invisible place: a character that comes into its box in the air takes hold and stays, and a jump lets go; the hang bars of the pre-beta's Sky Fortress are this and a picture of
// the level's own tiles). Each of the two characters has his own state. The pre-beta puts no script in it: the level's dynamic events are what use it when they place it.
void Obj_SFZHang(Sst *self);

#endif //_SFZOBJECTS_H
