#ifndef _BOSSBALL_H
#define _BOSSBALL_H

#include "Object.h"

// Object 48 - wrecking ball on a chain that Eggman swings (GHZ boss)
typedef struct {
    uint8_t child_count;   // 0x28 -- controller only: number of chain links (not counting the ball or self)
    uint8_t child_idx[6];    // 0x29+ -- controller only: RAM indices of [self, links..., ball]
    int16_t anchor_pos;         // 0x32 -- controller only: chain anchor's offset below the ship, drops to 32
    uint8_t parent_index;          // 0x34 -- all: index into objects[] for the GHZ boss ship
    int16_t orig_y;                   // 0x38 -- controller only (swing_origY)
    int16_t orig_x;                      // 0x3A -- controller only (swing_origX)
    uint8_t chain_length;                   // 0x3C -- link/ball only: current distance from the swing pivot
    uint8_t direction;                         // 0x3D -- controller only: 0 = clockwise, 1 = counterclockwise
    int16_t speed;                                // 0x3E -- controller only: current swing speed
    uint8_t angle_frac;                              // controller only: sub-angle accumulator (real hardware adds
                                                      // `speed` into a word straddling obAngle and this otherwise-
                                                      // unused adjacent byte -- only the high byte/obj->angle feeds
                                                      // CalcSine, so the effective turn rate is speed/256 per frame,
                                                      // not the raw speed value)
} Scratch_BossBall;

void Obj_BossBall(Object *obj);

#endif //_BOSSBALL_H
