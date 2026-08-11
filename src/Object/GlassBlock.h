#ifndef _GLASSBLOCK_H
#define _GLASSBLOCK_H

#include "Object.h"

typedef struct {
    // 0x28 (subtype, read directly via obj->scratch.u8[0]) -- upper
    // nibble = matching switch group (type 4 only), low 3 bits = movement
    // type (0-4), bit3 = set on the "sheen" companion object only
    int16_t orig_y;      // 0x30
    int16_t distance_y;    // 0x32 -- current Y-offset from orig_y; only meaningfully
                           // used by types 3/4 (types 0-2 compute their offset fresh
                           // from oscillation every frame instead)
    uint8_t flags;           // 0x34 -- type 3 (stomp): bit0 = Sonic standing on
                              // pillar, bit7 = currently moving down; type 4
                              // (switch): nonzero once the matching switch has
                              // been pressed
    uint8_t stomp_first;       // 0x35 -- type 3 only: set after the first stomp touch
    int16_t stomp_distance;      // 0x36 -- type 3 only: pixels left to descend from the current stomp
    uint8_t stomp_delay;           // 0x38 -- type 3 only: frames to wait before a stomp starts moving the pillar down
    uint8_t parent_index;            // 0x3C on real hardware (pointer there) -- index
                                      // into objects[] of the pillar this object
                                      // belongs to (every spawned piece, pillar
                                      // included, points at the original pillar)
} Scratch_GlassBlock;

void Obj_GlassBlock(Object *obj);

#endif //_GLASSBLOCK_H
