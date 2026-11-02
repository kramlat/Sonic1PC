#ifndef _CNZOBJECTS_H
#define _CNZOBJECTS_H

#include "Sst.h"

// Casino Night's objects of the alpha (none of them is placed in its layouts: they are for the zone as it went on): the round bumper (44)
void Obj_CNZBumper(Sst *self);
void Obj_AutoSpin(Sst *self);

// The pre-beta's green bumper (D8): three hits make it disappear, three of a group give a bonus; the groups' counts start over with the level
void Obj_CNZGreenBumper(Sst *self);
void CNZ_ResetBumperGroups(void);

// The pre-beta's blue bumper (D7): throws whoever touches it by the side he is on; with a subtype it goes to and fro over $C0 pixels
void Obj_CNZBlueBumper(Sst *self);

#endif //_CNZOBJECTS_H
