#ifndef _RING_H
#define _RING_H

#include "Sst.h"
#include "RingsManager.h"


// Ring formation offsets, indexed by (subtype>>4)&0xF -- see Ring.c's own
// spawn loop. Exposed here so Object/DebugList.c's placement preview can
// read the real values instead of keeping its own copy.
extern const int8_t ring_pos[16][2];

void Ring_Collect(void); // adds one ring: counter, sound, extra lives at 100 and 200

typedef struct {
    int16_t base_x; // 0x32
    uint8_t index; // 0x34
    uint16_t entry; // 0x36: the ring's entry in the ring layout, or RING_NO_ENTRY
} Own_Ring;

#define RING_NO_ENTRY 0xFFFF

// The ring layout's rings (RingsManager.h): started with the level, then given the camera's X once a frame (ObjPosLoad does both)
extern RingsManager rings_manager;
extern uint8_t ring_status[0x200];


void Obj_Ring(Sst *self);

void Obj_RingLoss(Sst *self);

#endif //_RING_H
