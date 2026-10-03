#ifndef _GIANTRING_H
#define _GIANTRING_H

#include "Object.h"

// Giant Ring / Flash assets
#include "Resource/Mappings/GiantRing.h"
#include "Resource/Mappings/RingFlash.h"

typedef struct {
    Object *parent; // parent Giant Ring object -- set to routine 6 (delete) once the flash reaches its 3rd frame
} Scratch_RingFlash;

void Obj_GiantRing(Object *obj);
void Obj_RingFlash(Object *obj);

#endif //_GIANTRING_H
