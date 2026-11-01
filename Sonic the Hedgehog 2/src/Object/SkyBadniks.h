#ifndef _SKYBADNIKS_H
#define _SKYBADNIKS_H

#include "Sst.h"

// The flying badniks of the pre-beta's Sky Fortress and Sky Chase and what goes with them: $9C the boost (the jet that follows a Turtloid, a Balkiry or a Clucker, drawn from the Turtloid's mappings and animated by the script
// its maker gives it) and $AC the Balkiry (it goes left, with its jet). The weapon object ($98: Coconuts.h) has the Turtloid's bullet and the Nebula's bomb in it.
void Obj_EnemyBoost(Sst *self);
void Obj_Balkiry(Sst *self);

// What the weapon object ($98) draws its Turtloid and Nebula shots from
const uint8_t *Turtloid_Mappings(void);
const uint8_t *Nebula_Mappings(void);
const uint8_t *Turtloid_ShotAnimation(void);

// Makes the boost of `parent` ($9C, with the animation script table it is to run and the frame it starts at)
void SkyBadniks_MakeBoost(Sst *parent, const uint8_t *animation);

// $AE: the Clucker (a chicken that stands on a platform ($AD) and, when a character comes within $80 of it, goes through its animations (it turns up, aims) and fires, every $40 frames after that: a shot
// of the weapon object)
void Obj_Clucker(Sst *self);

#endif //_SKYBADNIKS_H
