#ifndef _CHAINSTOMP_H
#define _CHAINSTOMP_H

#include "Object.h"

typedef struct {
    int16_t orig_y;      // 0x30
    uint16_t current;      // 0x32 -- current extension, 8.8 fixed point (high byte = integer pixels)
    uint16_t length;         // 0x34 -- max extension length, 8.8 fixed point (block only, meaningfully;
                             // copied to children too for fidelity, but nothing reads it there)
    bool rising;               // 0x36 -- auto-stomp types only: set while rising back up after a stomp
                               // (real hardware stores this as a full word flag, not a single bit)
    int16_t delay;               // 0x38 -- auto-stomp types only: frames left to wait before rising again
    uint8_t switch_id;              // 0x3A -- switch-activated type only: which f_switch entry triggers it
    uint8_t parent_index;             // 0x3C on real hardware (pointer there) -- children only: index
                                       // into objects[] of the main block this piece belongs to
} Scratch_ChainStomp;

void Obj_ChainStomp(Object *obj);

#endif //_CHAINSTOMP_H
