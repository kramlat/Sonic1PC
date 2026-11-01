#ifndef _CPZBOSS_H
#define _CPZBOSS_H

#include "Sst.h"

// The boss of Chemical Plant for Sonic 2 (the pre-beta's object $5D, with the events of the end of its second act): Eggman's ship, which goes from one side of the arena to the other and puts out a pipe that
// swings over Sonic and drops a blob of the chemical on him. One object has all of its parts, each with a routine of its own (the pre-beta does the same: a part is an object of the boss's id)
void Obj_CPZBoss(Sst *self);

// The arena at the end of Chemical Plant's second act: the camera is locked, the boss's art and its music come, and when it is beaten the results card
void CPZBoss_Events(void);

#endif //_CPZBOSS_H
