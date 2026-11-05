#ifndef _SHIELDINVINCIBILITY_H
#define _SHIELDINVINCIBILITY_H

#include "Sst.h"

typedef struct {
    uint8_t trail; // 0x30
} Own_Invincibility;

// Gives Sonic the shield (anim 0) or one of the four invincibility stars (anim 1-4): the object of that kind that is already out there stays, otherwise a new one is made
void ShieldInvincibility_Make(uint8_t anim);

void Obj_ShieldInvincibility(Sst *self);

#endif //_SHIELDINVINCIBILITY_H
