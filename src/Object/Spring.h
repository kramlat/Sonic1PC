#ifndef _SPRING_H
#define _SPRING_H

#include "Object.h"

// Spring assets
#include "Resource/Animation/Spring.h"
#include "Resource/Mappings/Spring.h"

typedef struct {
    uint8_t subtype; // 0x28
    uint8_t pad[7]; // 0x29-0x2F
    int16_t power; // 0x30
} Scratch_Spring;

#endif //_SPRING_H
