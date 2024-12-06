#ifndef _CRABMEAT_H
#define _CRABMEAT_H

#include "Object.h"

#include "Level.h"
#include "LevelCollision.h"

// Crabmeat assets
#include "Resource/Animation/Crabmeat.h"
#include "Resource/Mappings/Crabmeat.h"

// Crabmeat object
typedef struct {
    uint8_t pad0[8]; // 0x28-0x2F
    int16_t time_delay; // 0x30
    uint8_t crab_mode; // 0x32
} Scratch_Crabmeat;

static uint8_t Obj_Crabmeat_SetAni(Object* obj);
void Obj_Crabmeat_Construct(Object* obj);
void Obj_Crabmeat_Fall(Object* obj, int16_t floor_dist);
bool Obj_Crabmeat_Check_Timer(Scratch_Crabmeat* scratch);
void Obj_Crabmeat_TurnAround(Object* obj, Scratch_Crabmeat* scratch);
void Obj_Crabmeat_Fire_Projectile(Object* obj, Object* proj, uint8_t type, uint8_t routine, int16_t x, int16_t y, int16_t xsp);
void Obj_Crabmeat_Fire(Object* obj, Scratch_Crabmeat* scratch);
bool Obj_Crabmeat_Check_CrabMode(Scratch_Crabmeat* scratch);
bool Obj_Crabmeat_Check_EdgeDist(int16_t floor_dist);
int16_t Obj_Crabmeat_Get_EdgeDist(Object* obj);
void Obj_Crabmeat_NonZIF(Object* obj, int16_t floor_dist);
void Obj_Crabmeat_Stop(Object* obj, Scratch_Crabmeat* scratch);
void Obj_Crabmeat_Projectile_Construct(Object* obj);
bool Obj_Crabmeat_Check_BelowLevel(Object* obj);
void Obj_Crabmeat(Object* obj);

#endif //_CRABMEAT_H
