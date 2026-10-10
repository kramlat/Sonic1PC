#ifndef _COUNTDOWN_H
#define _COUNTDOWN_H

#include "Object.h"

// Object 0A as the alpha has it (the breathing bubbles and the countdown of the air, for Sonic and for Tails). The alpha keeps each player's air in his own object (the byte at $28, Scratch_Sonic's air),
// which is how Tails gets a countdown too. Not in the object table yet: the alpha's art for the bubbles (tile $55B) is in its zone art lists, which Sonic 2 does not have yet, so Sonic 1's DrownCount
// still counts for Sonic, and this one waits for them.
#define COUNTDOWN_MASTER_BIT 0x80 // (the subtype of the one that counts: its low bits are the bubbles' pause)

void Obj_Countdown(Object *obj);

// Makes the counting object for a player (slot `slot`), who is Tails if `tails`; he must be underwater for it to start counting
void Countdown_Make(uint8_t slot, bool tails);

// What the alpha does when a player has air again (Resume_Music): the level's music back if he was in the last 12 and his air to 30
void Countdown_ResumeMusic(Object *player);

#endif //_COUNTDOWN_H
