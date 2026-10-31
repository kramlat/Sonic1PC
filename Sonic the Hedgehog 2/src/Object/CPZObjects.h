#ifndef _CPZOBJECTS_H
#define _CPZOBJECTS_H

#include "Sst.h"

void Obj_CPZElevator(Sst *self);
void Obj_CPZBooster(Sst *self);
void Obj_CPZPipeTipper(Sst *self);
void Obj_CPZBlock(Sst *self);

// A badnik's score for something broken, and breaking an object into the pieces of its frame (the object is the first piece): shared by the objects that break (Hill Top's too)
void ObjectChainScore(Sst *self);
void ObjectBreakToPieces(Sst *self, const int16_t (*speeds)[2], int count);
void Obj_CPZBarrier(Sst *self);
void Obj_TubeCover(Sst *self);
void Obj_CPZRotor(Sst *self);
void Obj_CPZSlider(Sst *self);
void Obj_CPZInvisibleBlock(Sst *self);
void Obj_CPZWorm(Sst *self);
void Obj_FloatingPlatform(Sst *self);

#endif //_CPZOBJECTS_H
