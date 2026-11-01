#ifndef _CPZBADNIKS_H
#define _CPZBADNIKS_H

#include "Sst.h"

// Chemical Plant's badniks (no layout of it places them): the Spiny (the pre-beta's Lander: A5 on the floor, A6 on a wall: it goes to and fro, and when Sonic comes near it stops and throws a shot, which the weapon
// object 98 carries) and the Grabber (A7 and its parts A8 the claw, A9 the head, AA the thread: the pre-beta's spider, which takes whoever its claw touches up with it, and explodes unless he is shaken free)
void Obj_Spiny(Sst *self);
void Obj_SpinyVertical(Sst *self);
void Obj_Grabber(Sst *self);
void Obj_GrabberClaw(Sst *self);
void Obj_GrabberHead(Sst *self);
void Obj_GrabberStrand(Sst *self);
void Obj_GrabberUnused(Sst *self);

const uint8_t *Spiny_Mappings(void);   // (the weapon object makes the shot with them)
const uint8_t *Spiny_ShotAnimation(void);

#endif //_CPZBADNIKS_H
