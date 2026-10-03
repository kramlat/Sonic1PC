#ifndef _TITLECARD_H
#define _TITLECARD_H

#include "Object.h"

// Title card mappings
#ifndef Titlecard_Build
extern const uint8_t Mappings_TitleCard
extern const uint8_t GotThrough_TitleCard
extern const uint8_t SpecialResult_TitleCard
#endif

typedef struct {
    uint8_t pad[8]; // 0x28-0x2F
    int16_t main_x; // 0x30
    int16_t final_x; // 0x32
} Scratch_TitleCard;

#endif //_TITLECARD_H
