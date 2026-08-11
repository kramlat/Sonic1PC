#ifndef _GEYSERMAKER_H
#define _GEYSERMAKER_H

#include "Object.h"

typedef struct {
    uint8_t pad0[10];  // 0x28-0x31
    int16_t timer;       // 0x32 -- countdown to the next eruption
    int16_t time;          // 0x34 -- interval between eruptions (fixed, 120 frames)
    uint8_t parent_index;    // 0x3C on real hardware (pointer there) -- subtype 0
                              // (geyser) only: index into objects[] of the parent
                              // PushBlock this maker was spawned by. Real hardware
                              // only ever spawns subtype-0 makers this way (see
                              // PushBlock.c); a subtype-0 maker placed directly in
                              // a level layout would misbehave on real hardware
                              // too, reading whatever this defaults to.
} Scratch_GeyserMaker;

void Obj_GeyserMaker(Object *obj);

#endif //_GEYSERMAKER_H
