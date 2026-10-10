#ifndef _COCONUTS_H
#define _COCONUTS_H

#include "Object.h"

// Emerald Hill's Coconuts (the alpha's object 9D): a monkey that climbs up and down a palm trunk and throws coconuts at Sonic, and its weapon (object 98: the alpha's one object for what its badniks throw; only the
// coconut is ported so far).
#define ObjId_Coconuts    0x9D
#define ObjId_EnemyWeapon 0x98

// What a weapon object (98) is: it is made with its kind in its scratch memory (the alpha keeps the address of the routine it runs there)
typedef enum { Weapon_Coconut, Weapon_AsteronSpike } Weapon;

typedef struct {
    uint8_t subtype; // 0x28
    uint8_t weapon;  // 0x29: a Weapon
} Scratch_Weapon;

void Obj_Coconuts(Object *obj);
void Obj_EnemyWeapon(Object *obj);

#endif //_COCONUTS_H
