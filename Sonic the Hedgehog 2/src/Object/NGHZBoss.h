#ifndef _NGHZBOSS_H
#define _NGHZBOSS_H

#include "Object.h"

// Neo Green Hill's boss of the alpha (object 89, which makes its own parts: its routine 2 is the ship, 4 the pillars' launcher and the arrow-pillars that it lets fall): Robotnik's ship goes to and fro over the
// arena with a hammer, and each time it turns the walls throw an arrow-pillar across. Defeated, it blows up and the ship leaves (in the alpha the camera's right edge goes on opening and nothing ends the act: the
// code that does it checks for the value of Dust Hill's arena).
#define ObjId_NGHZBoss 0x89

void Obj_NGHZBoss(Object *obj);

// DynResize_NGHz of act 2 (nothing in act 1): the camera stops at the arena and the boss comes
void NGHZBoss_Events(void);

#endif //_NGHZBOSS_H
