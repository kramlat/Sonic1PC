#ifndef _CPZOBJECTS_H
#define _CPZOBJECTS_H

#include "Object.h"

void Obj_CPZElevator(Object *obj);
void Obj_CPZBooster(Object *obj);
void Obj_CPZPipeTipper(Object *obj);
void Obj_CPZBlock(Object *obj);

// A badnik's score for something broken, and breaking an object into the pieces of its frame (the object is the first piece): shared by the objects that break (Hill Top's too)
void ObjectChainScore(Object *obj);
void ObjectBreakToPieces(Object *obj, const int16_t (*speeds)[2], int count);
void Obj_CPZBarrier(Object *obj);
void Obj_TubeCover(Object *obj);
void Obj_CPZRotor(Object *obj);
void Obj_CPZSlider(Object *obj);
void Obj_CPZInvisibleBlock(Object *obj);
void Obj_CPZWorm(Object *obj);
void Obj_FloatingPlatform(Object *obj);

#endif //_CPZOBJECTS_H
