#pragma once

#include "Sst.h"

// Object 0x0A - LZ drowning countdown. Two roles, told apart by whether
// obSubtype's bit 7 is set (see s1disasm's "0A LZ Drowning Countdown.asm"):
//
//  - The "master" tracker, the one instance (DrownCount_Master) riding
//    for as long as Sonic's underwater (spawned by Sonic_Water in Sonic.c
//    with subtype DROWNCOUNT_MASTER_BIT|1). Ticks the air countdown, plays
//    the warning stingers/drowning music, and spawns the visible bubbles/
//    number bubbles below as short-lived child objects. Never displays a
//    sprite of its own.
//  - A visible child bubble/number bubble (subtype 0-5 = countdown digit
//    0-5, 6 = small bubble, 0xE = medium bubble), sharing Object/AirBubbles'
//    Mappings_Bubbles table -- countdown digits are just extra frames in
//    that same table, not a separate art asset.
#define DROWNCOUNT_MASTER_BIT 0x80

extern const uint8_t Animation_DrowningCountdown[];

void Obj_DrownCount(Sst *self);

// The master tracker (NULL when there is none), and starting it when Sonic enters the water
Sst *DrownCount_Master(void);
void DrownCount_StartMaster(void);

// Replenishes air to 30s and (if the drowning countdown/music has kicked
// in) restores the appropriate music -- called on water exit, air bubble
// pickup, and level start/respawn. Matches s1disasm's sub ResumeMusic.asm.
void ResumeMusic(void);
