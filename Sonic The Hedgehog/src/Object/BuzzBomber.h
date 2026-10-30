#ifndef _BUZZBOMBER_H_
#define _BUZZBOMBER_H_

#include "Sst.h"

// Buzz Bomber's missile (object 23; Newtron's too, by its subtype)
typedef struct {
    int16_t time_delay;
    uint8_t parent_index; // the table slot of the Buzz Bomber that fired it
} Own_BuzzMissile;

// Buzz Bomber object
typedef struct {
    int16_t time_delay;
    int16_t buzz_status;
} Own_BuzzBomber;

// Buzz Bomber's missile blowing up (object 24): the dissolving animation of the original is not ported yet, the missile just goes
void Obj_BuzzExplode(Sst *self);
void Obj_BuzzMissile(Sst *self);
void Obj_BuzzBomber(Sst *self);

#endif //_BUZZBOMBER_H_
