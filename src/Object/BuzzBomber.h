#ifndef _BUZZBOMBER_H
#define _BUZZBOMBER_H_

#include "Object.h"

#include "Level.h"

// Buzz Bomber assets
#include "Resource/Mappings/BuzzBomber.h"
#include "Resource/Mappings/BuzzMissile.h"
/*
        #include "Resource/Mappings/BuzzExplode.h"
*/

#include "Resource/Animation/BuzzBomber.h"
#include "Resource/Animation/BuzzMissile.h"

// Buzz Bomber's missile
typedef struct {
    uint8_t subtype; // 0x28
    uint8_t pad0[9]; // 0x29 - 0x31
    int16_t time_delay; // 0x32
    uint8_t pad1[0xC - sizeof(Object*)]; // This will break in 20 years when 128-bit processors are mainstream
    Object* parent; // 0x3C assuming 32-bit address
} Scratch_BuzzMissile;

// Buzz Bomber object
typedef struct
{
    uint8_t pad0[10]; // 0x28-0x31
    int16_t time_delay; // 0x32
    int16_t buzz_status; // 0x34
} Scratch_BuzzBomber;

void Obj_BuzzExplode(Object* obj);

void Obj_BuzzMissile_Construct(Object* obj);
bool Obj_BuzzMissile_CheckNewtron(Object* obj, Scratch_BuzzMissile* scratch);
void Obj_BuzzMissile_Charge(Object* obj, Scratch_BuzzMissile* scratch);
void Obj_BuzzMissile_Fire(Object* obj);
void Obj_BuzzMissile_NewtFire(Object* obj);
void Obj_BuzzMissile(Object* obj);

void Obj_BuzzBomber_Construct(Object* obj);
void Obj_BuzzBomber_Fly(Object* obj, Scratch_BuzzBomber* scratch);
void Obj_BuzzBomber_Fire(Object* obj, Object* missile, Scratch_BuzzBomber* scratch);
bool Obj_BuzzBomber_CheckCloseToSonic(Object* obj);
void Obj_BuzzBomber_SetStatusAttack(Scratch_BuzzBomber* scratch);
void Obj_BuzzBomber_TurnAround(Object* obj, Scratch_BuzzBomber* scratch);
void Obj_BuzzBomber_Stop(Object* obj);
void Obj_BuzzBomber(Object* obj);

#endif
