#ifndef _TAILS_H
#define _TAILS_H

#include <stdbool.h>

#include "Sst.h"

#define TAILS_SLOT 0x1C // Tails' fixed object slot (the sidekick)

// Tails, for the code that has to treat him like Sonic (the solid objects touch both)
void Tails_LoadPilotFrame(uint8_t frame);
#define TAILS_OBJ SST_PLAYER_2 // Tails: the second character (Player_2)
// What Sonic.c records for each frame beside his position (track_sonic): his pad, held and pressed, and his status; Tails' CPU follows them
extern uint8_t sonic_rec_hold[0x40], sonic_rec_press[0x40], sonic_rec_status[0x40];
// Tails' flight and carrying (the Sonic Retro guide's, Sonic 3 style): Sonic hangs from him (the secondary status's carry bit)
extern bool sonic_carried;
bool Tails_IsFlying(void);                    // his flight flag is set
bool Tails_IsRespawning(void);                // he is flying in to Sonic after being left behind
void Tails_CallToCarry(void);                 // Sonic's jump and Up (the single player assist): the AI Tails is to fly and carry him
void Sonic_CarryCollision(Sst *sonic);        // Sonic's collision while he is carried (Sonic.c)
bool Sonic_Spindashing(void);                 // Sonic is charging a spin dash (Sonic.c): Tails does not take him then
#define SONIC_ANIM_HANG2_ID 0x20              // the animation of Sonic hanging from Tails
void Tails_CancelSpindash(void);            // drops a charging spin dash of Tails'
void Tails_ResetOnFloor(Sst *self);
void KillTails(Sst *self);
int32_t Tails_Hurt(Sst *self, Sst *src); // (hurts Tails as an object hurts Sonic: HurtSonic in Sonic.c)

#endif //_TAILS_H
