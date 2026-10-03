#ifndef _JAWS_H
#define _JAWS_H

#include "Object.h"

typedef struct {
    uint8_t pad0[8]; // 0x28-0x2F
    int16_t turndelay_current; // 0x30 -- delay before turning around (64 frames per subtype value)
    int16_t turndelay_base;    // 0x32 -- base turn delay to reset to on turn
} Scratch_Jaws;

void Obj_Jaws(Object *obj);

#endif //_JAWS_H
