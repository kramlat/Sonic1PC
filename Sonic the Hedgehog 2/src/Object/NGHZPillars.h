#ifndef _NGHZPILLARS_H
#define _NGHZPILLARS_H

#include "Sst.h"

// Neo Green Hill's pillars of the Sonic 2 pre-beta: 23 (drops its lower part when a character comes near), 2B (rises when one does, breaks when he stands on it) and 82 (falls, rises or follows the water)
void Obj_NGHZPillar(Sst *self);
void Obj_NGHZRisingPillar(Sst *self);
void Obj_NGHZFallingPillar(Sst *self);

#endif //_NGHZPILLARS_H
