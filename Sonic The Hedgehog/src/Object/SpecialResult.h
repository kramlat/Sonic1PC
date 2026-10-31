#ifndef _SPECIALRESULT_H
#define _SPECIALRESULT_H

#include "Sst.h"

// Object 7E - Special Stage results card; Object 7F - the Chaos Emeralds shown on it. Ported from s1disasm's
// "_incObj/7E, 7F Special Stage Results and Chaos Emeralds.asm". The card is built from the object that is spawned
// (the header text) and the ones it makes after it; the emeralds are made by the ring bonus element.

typedef struct {
    int16_t main_x;   // X the element slides in to
    uint16_t timer;   // frames it waits (frame_time.w)
} Own_SpecialResult;

void Obj_SpecialResult(Sst *self);
void Obj_SpecialResultEmerald(Sst *self);

#endif //_SPECIALRESULT_H
