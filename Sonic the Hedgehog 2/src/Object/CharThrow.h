#ifndef _CHARTHROW_H
#define _CHARTHROW_H

#include "Object.h"
#include "Object/Sonic.h"

// How the prototype's springs, steam vents and pushers throw a character that is tumbling (the object's subtype: bit 0 tumbles, bit 1 a short tumble, bits 2 and 3 pick the collision path): the flips and
// their speed, and the path's solid bits. `flips_short` and `flips_long` are the number of turns for the two lengths
static inline void CharThrow_Tumble(Object *chr, uint8_t subtype, uint8_t flip_speed, uint8_t flips_short, uint8_t flips_long) {
    Scratch_Sonic *sscratch = (Scratch_Sonic *)&chr->scratch;
    if (subtype & 1) {
        chr->inertia = 1;
        sscratch->flip_angle = 1;
        chr->anim = SonAnimId_Walk;
        sscratch->flips_remaining = flips_short;
        sscratch->flip_speed = flip_speed;
        if (!(subtype & 2))
            sscratch->flips_remaining = flips_long;
        if (chr->status.p.f.x_flip) {
            sscratch->flip_angle = (uint8_t)-sscratch->flip_angle;
            chr->inertia = -chr->inertia;
        }
    }
    switch (subtype & 0xC) {
    case 4:
        sscratch->top_solid_bit = 0xC;
        sscratch->lrb_solid_bit = 0xD;
        break;
    case 8:
        sscratch->top_solid_bit = 0xE;
        sscratch->lrb_solid_bit = 0xF;
        break;
    }
}

#endif //_CHARTHROW_H
