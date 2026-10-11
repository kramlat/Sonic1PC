#ifndef _CPZBADNIKS_H
#define _CPZBADNIKS_H

#include "Object.h"

// Chemical Plant's badniks of the alpha (no layout of it places them): the Spiny (A5 on the floor, A6 on a wall: it goes to and fro, and when Sonic comes near it stops and throws a shot, which the weapon
// object 98 carries) and the Grabber (A7 and its parts A8-AB: in the alpha they only set themselves up and stay: the spider does nothing yet)
void Obj_Spiny(Object *obj);
void Obj_SpinyVertical(Object *obj);
void Obj_Grabber(Object *obj);

const uint8_t *Spiny_Mappings(void);   // (the weapon object makes the shot with them)
const uint8_t *Spiny_ShotAnimation(void);

#endif //_CPZBADNIKS_H
