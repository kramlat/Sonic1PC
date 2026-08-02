#ifndef _SHIELDINVINCIBILITY_H
#define _SHIELDINVINCIBILITY_H

#include "Object.h"

// Shield and invincibility assets
#include "Resource/Mappings/ShieldInvincibility.h"
#include "Resource/Animation/ShieldInvincibility.h"

typedef struct {
    uint8_t pad[0x8]; // 0x28-0x2F
    uint8_t trail; // 0x30
} Scratch_Invincibility;

#endif //_SHIELDINVINCIBILITY_H
