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

// The pre-beta's elevator (D5): a platform that rides to the other end of its run when it is stood on, and back when it is left
void Obj_CNZElevator(Sst *self);

// The pre-beta's blue box (D4): a solid box that swings $60 pixels either side of where it is put, sideways (subtype 0) or up and down (2)
void Obj_CNZBlueBox(Sst *self);

// The pre-beta's green platform (D2): a line of squares that turns about its place, a frame every 16 ticks and a wait between turns (the subtype times 16)
void Obj_CNZGreenPlatform(Sst *self);

// The pre-beta's Crawl (C8): a guard that turns to a character within $60 either way and throws whoever touches it, as a bumper does
void Obj_CNZCrawl(Sst *self);

#endif //_CNZOBJECTS_H
