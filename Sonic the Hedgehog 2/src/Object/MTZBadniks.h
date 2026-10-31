#ifndef _MTZBADNIKS_H
#define _MTZBADNIKS_H

#include "Sst.h"

// Metropolis's badniks of the alpha: the Shellcracker (object 9F) and the claw that it shoots out (A0), and the Asteron (A4), which blows up into the spikes it throws (weapons of object 98, see Coconuts.h)
#define ObjId_Shellcracker 0x9F
#define ObjId_ShellcrackerClaw 0xA0
#define ObjId_Asteron 0xA4
#define ObjId_Slicer 0xA1
#define ObjId_SlicerPincers 0xA2

void Obj_Shellcracker(Sst *self);
void Obj_ShellcrackerClaw(Sst *self);
void Obj_Asteron(Sst *self);
void Obj_Slicer(Sst *self);
void Obj_SlicerPincers(Sst *self);

// The Asteron's mappings, for the spikes that it throws
const uint8_t *Asteron_Mappings(void);

#endif //_MTZBADNIKS_H
