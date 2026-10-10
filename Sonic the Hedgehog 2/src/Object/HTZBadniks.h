#ifndef _HTZBADNIKS_H
#define _HTZBADNIKS_H

#include "Object.h"

// Hill Top's badniks of the alpha: the Spiker (object 92, which fires a drill, 93, up or down when Sonic is above or below it), the Sol (95: a ball of fire with four fireballs circling it, that it throws at Sonic when he is
// near) and the Rexon (96 body, 97 head).
#define ObjId_Spiker 0x92
#define ObjId_SpikerDrill 0x93
#define ObjId_Sol 0x95
#define ObjId_RexonBody 0x96
#define ObjId_RexonHead 0x97

void Obj_Spiker(Object *obj);
void Obj_SpikerDrill(Object *obj);
void Obj_Sol(Object *obj);
void Obj_RexonBody(Object *obj);
void Obj_RexonHead(Object *obj);

// The Rexon's mappings, for the fireball it throws
const uint8_t *Rexon_Mappings(void);

#endif //_HTZBADNIKS_H
