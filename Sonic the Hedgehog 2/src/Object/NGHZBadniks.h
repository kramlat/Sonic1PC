#ifndef _NGHZBADNIKS_H
#define _NGHZBADNIKS_H

#include "Object.h"

// Neo Green Hill's badniks of the alpha, which no layout places: the Whisp (8C), the Grounder (8D hidden in its wall, 8E out in the open) with the wall's bricks (8F) and the rocks it throws (90), and the Chop Chop (91)
void Obj_Whisp(Object *obj);
void Obj_Grounder(Object *obj);
void Obj_GrounderWall(Object *obj);
void Obj_GrounderRock(Object *obj);
void Obj_ChopChop(Object *obj);

#endif //_NGHZBADNIKS_H
