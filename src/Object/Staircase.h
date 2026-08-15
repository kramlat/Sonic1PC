#ifndef _STAIRCASE_H
#define _STAIRCASE_H

#include "Object.h"

typedef struct {
    uint8_t pad0[8];       // 0x28-0x2F (0x28 = subtype, copied from parent to every child)
    int16_t orig_x;          // 0x30
    int16_t orig_y;            // 0x32
    int16_t delay;               // 0x34 -- parent only: delay between activation and stairs moving down
    int8_t touch;                  // 0x36 -- parent only: touch state written by children (0 = none, >0 = from above, <0 = from below)
    uint8_t parent_y_index;          // 0x37 -- child only: which of parent's children_y[] slots this block reads
    uint8_t children_y[4];             // 0x38-0x3B -- parent only: relative Y-positions for the 4 child blocks
    uint8_t parent_index;                // 0x3C on real hardware (pointer there) -- child only: index into objects[] for the parent (also set on the parent itself, pointing to itself, but never read since the parent never runs Stair_Solid)
} Scratch_Staircase;

void Obj_Staircase(Object *obj);

#endif //_STAIRCASE_H
