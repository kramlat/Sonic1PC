#ifndef _SPECIALRESULT_H
#define _SPECIALRESULT_H

#include "Object.h"

// Object 7E - Special Stage results card; Object 7F - the Chaos Emeralds shown on it. Ported from s1disasm's
// "_incObj/7E, 7F Special Stage Results and Chaos Emeralds.asm". The card is built from consecutive objects
// starting at objects[SSR_CARD_SLOT]; the emeralds from objects[SSR_EMERALD_SLOT].
#define SSR_CARD_SLOT    23 // v_ssrescard
#define SSR_EMERALD_SLOT 32 // v_ssresemeralds

typedef struct {
    uint8_t pad[8];   // 0x28-0x2F
    int16_t main_x;   // 0x30 -- X the element slides in to
} Scratch_SpecialResult;

void Obj_SpecialResult(Object *obj);
void Obj_SpecialResultEmerald(Object *obj);

#endif //_SPECIALRESULT_H
