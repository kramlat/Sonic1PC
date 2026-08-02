#ifndef _TITLECARD_H
#define _TITLECARD_H

#include "Object.h"

// Title card mappings
#include "Resource/Mappings/TitleCard.h"

typedef struct {
    uint8_t pad[8]; // 0x28-0x2F
    int16_t main_x; // 0x30
    int16_t final_x; // 0x32
} Scratch_TitleCard;

#endif //_TITLECARD_H
