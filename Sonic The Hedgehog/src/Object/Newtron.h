#ifndef _NEWTRON_H
#define _NEWTRON_H

#include "Object.h"

// Newtron assets
#include "Resource/Animation/Newtron.h"
#include "Resource/Mappings/Newtron.h"

// Newtron object
typedef struct {
    uint8_t subtype; // 0x28
    uint8_t pad[0x9]; // 0x29-0x31
    uint8_t fired; // 0x32
} Scratch_Newtron;

#endif //_NEWTRON_H
