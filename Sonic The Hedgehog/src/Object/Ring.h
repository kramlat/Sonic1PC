#ifndef _RING_H
#define _RING_H

#include "Sst.h"
#include "Rings.h"


// Ring formation offsets, indexed by (subtype>>4)&0xF -- see Ring.c's own
// spawn loop. Exposed here so Object/DebugList.c's placement preview can
// read the real values instead of keeping its own copy.
extern const int8_t ring_pos[16][2];

void Ring_Collect(void); // adds one ring: counter, sound, extra lives at 100 and 200

typedef struct {
    int16_t base_x; // 0x32
    uint8_t index; // 0x34
} Own_Ring;

// The level's rings (Rings.h): ObjPosLoad starts them with the level; the player's touch routine collects the ones it touches
void Ring_StartLayout(const uint8_t *layout);
void Ring_TouchLayout(const Sst *player);


void Obj_Ring(Sst *self);

void Obj_RingLoss(Sst *self);

#endif //_RING_H
