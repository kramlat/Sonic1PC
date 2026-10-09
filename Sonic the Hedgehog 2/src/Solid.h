#ifndef _SOLID_H
#define _SOLID_H

#include "Object.h"

// Solid objects for both characters, as Nick Arcade does them (SolidObject_Always_SingleCharacter / SlopedSolid_SingleCharacter): Sonic stands on an object with its status bit 3 and pushes
// it with bit 5, Tails with bits 4 and 6. Sonic 1's SolidObject (Object.c) only knows Sonic; this is for the objects that Sonic 2 replaces.
enum { SolidChar_Sonic = 0, SolidChar_Tails = 1 };

// The character `chr` against `obj`: x_rad is half its width, y_air and y_walk half its height for a character in the air and on the ground, x its x position (as of before it moved).
// slope is NULL for a flat top, or the height of each 2 pixel column of the top (a diagonal spring's).
// Returns 0 for nothing, 1 for a side, -1 for standing on top, -2 for the underside.
int32_t Solid_Character(Object *obj, Object *chr, int who, int16_t x_rad, int16_t y_air, int16_t y_walk, int16_t x, const int8_t *slope);

// The same against an object whose top and thickness both change across it (DoubleSlopedSolid): `table` has two signed bytes for every two pixels, how high the top is above the object's centre and how thick
// it is there
int32_t Solid_CharacterDouble(Object *obj, Object *chr, int who, int16_t x_rad, int16_t x, const int8_t *table);

// Landing on a platform's top: see Solid.c. True if the character has just landed.
bool Solid_PlatformLand(Object *obj, Object *chr, int who, int16_t x_rad, int16_t width, int16_t y_walk);

// A platform for the character `who`: carried while standing on it, landing on it otherwise (see Solid.c)
void Solid_Platform(Object *obj, Object *chr, int who, int16_t x_rad, int16_t y_walk, int16_t x);

// A platform with a sloped top for the character `who` (see Solid.c)
void Solid_SlopedPlatform(Object *obj, Object *chr, int who, int16_t x_rad, const uint8_t *slope, int16_t x);

// Puts the character on the object as if he had landed on it (see Solid.c)
void Solid_Ride(Object *obj, Object *chr, int who);

#endif //_SOLID_H
