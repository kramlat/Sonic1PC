#ifndef _CROCOBOT_H
#define _CROCOBOT_H

#include "Object.h"

// Object 4E: the Crocobot, Sand Shower's walking badnik (the staged objgator.asm: it comes from the final game's source). It drops to the floor, walks to and fro (turning at ledges, after a wait of $3B frames) and
// opens its mouth when Sonic is in front of it, within $40 pixels.
#define ObjId_Crocobot 0x4E

void Obj_Crocobot(Object *obj);

#endif //_CROCOBOT_H
