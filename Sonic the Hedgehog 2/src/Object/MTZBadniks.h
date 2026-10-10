#ifndef _MTZBADNIKS_H
#define _MTZBADNIKS_H

#include "Object.h"

// Metropolis's badniks of the alpha: the Shellcracker (object 9F) and the claw that it shoots out (A0), and the Asteron (A4), which blows up into the spikes it throws (weapons of object 98, see Coconuts.h)
#define ObjId_Shellcracker 0x9F
#define ObjId_ShellcrackerClaw 0xA0
#define ObjId_Asteron 0xA4
#define ObjId_Slicer 0xA1
#define ObjId_SlicerPincers 0xA2

void Obj_Shellcracker(Object *obj);
void Obj_ShellcrackerClaw(Object *obj);
void Obj_Asteron(Object *obj);
void Obj_Slicer(Object *obj);
void Obj_SlicerPincers(Object *obj);

// The Asteron's mappings, for the spikes that it throws
const uint8_t *Asteron_Mappings(void);

#endif //_MTZBADNIKS_H
