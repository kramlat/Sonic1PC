#ifndef _MOTOBUG_H
#define _MOTOBUG_H

#include "Object.h"

// Motobug assets
#include "Resource/Mappings/Motobug.h"
#include "Resource/Animation/Motobug.h"

typedef struct {
    uint8_t pad0[8]; // 0x28-0x2F
    int16_t time; // 0x30
    uint8_t pad1; // 0x32
    int8_t smoke_delay; // 0x33
} Scratch_Motobug;

#endif //_MOTOBUG_H
