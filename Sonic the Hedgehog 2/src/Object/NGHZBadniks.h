#ifndef _NGHZBADNIKS_H
#define _NGHZBADNIKS_H

#include "Sst.h"

// Neo Green Hill's badniks of the alpha, which no layout places: the Whisp (8C), the Grounder (8D hidden in its wall, 8E out in the open) with the wall's bricks (8F) and the rocks it throws (90), and the Chop Chop (91)
void Obj_Whisp(Sst *self);
void Obj_Grounder(Sst *self);
void Obj_GrounderWall(Sst *self);
void Obj_GrounderRock(Sst *self);
void Obj_ChopChop(Sst *self);

#endif //_NGHZBADNIKS_H
