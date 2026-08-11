#ifndef _CATERKILLER_H
#define _CATERKILLER_H

#include "Object.h"

typedef struct {
    uint8_t waittime;    // 0x2A -- time to wait between actions
    uint8_t mode;         // 0x2B -- bit4 = mouth open/segment moving up, bit7 = update animation
    uint8_t floormap[16]; // 0x2C-0x3B -- circular per-position floor-height history, read one step
                           // later by whichever segment follows this one (or by this same object's
                           // own next body segment, for the head)
    // Real hardware packs a body segment's parent pointer and its own
    // segmentpos counter into the SAME 4 bytes (0x3C), relying on the
    // 68000's 24-bit address bus silently discarding whatever segmentpos
    // scribbles into the pointer's otherwise-always-zero top byte -- not a
    // trick that survives on a real (64-bit) host pointer, so these are
    // kept as two honest, separate fields here instead.
    uint8_t parent_index; // 0x3C on real hardware -- body segments only: index into objects[] of this segment's own parent (head or previous segment)
    uint8_t segmentpos;   // 0x3C on real hardware -- position counter (0/4/8/0xA at spawn), incremented as this object moves; used by both the head (into its own floormap) and body segments (into their own AND their parent's floormap)
} Scratch_Caterkiller;

void Obj_Caterkiller(Object *obj);

#endif //_CATERKILLER_H
