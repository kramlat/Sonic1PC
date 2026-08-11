#ifndef _LAVAGEYSER_H
#define _LAVAGEYSER_H

#include "Object.h"

typedef struct {
    int16_t orig_y;      // 0x30
    uint8_t parent_index;  // 0x3C on real hardware (pointer there) -- meaning
                            // depends on which of the (up to) 3 spawned pieces
                            // this is: the "top" bubbling tip's own parent is the
                            // GeyserMaker that spawned it; the "middle" lava
                            // wall's parent is the top tip; the (lavafall-only)
                            // "bottom" tip's parent is also the GeyserMaker
                            // (copied from the top tip's own parent reference,
                            // not the top tip itself)
} Scratch_LavaGeyser;

void Obj_LavaGeyser(Object *obj);

#endif //_LAVAGEYSER_H
