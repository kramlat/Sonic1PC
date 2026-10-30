#ifndef _CRABMEAT_H
#define _CRABMEAT_H

#include "Sst.h"

#include "Level.h"
#include "LevelCollision.h"

// Crabmeat object
typedef struct {
    int16_t time_delay; // 0x30
    uint8_t crab_mode; // 0x32
} Own_Crabmeat;

static uint8_t Obj_Crabmeat_SetAni(Sst *self);
void Obj_Crabmeat_Construct(Sst *self);
void Obj_Crabmeat_Fall(Sst *self, int16_t floor_dist);
bool Obj_Crabmeat_Check_Timer(Own_Crabmeat *scratch);
void Obj_Crabmeat_TurnAround(Sst *self, Own_Crabmeat *scratch);
void Obj_Crabmeat_Fire_Projectile(Sst *self, uint8_t routine, int16_t x, int16_t y, int16_t xsp);
void Obj_Crabmeat_Fire(Sst *self, Own_Crabmeat *scratch);
bool Obj_Crabmeat_Check_CrabMode(Own_Crabmeat *scratch);
bool Obj_Crabmeat_Check_EdgeDist(int16_t floor_dist);
int16_t Obj_Crabmeat_Get_EdgeDist(Sst *self);
void Obj_Crabmeat_NonZIF(Sst *self, int16_t floor_dist);
void Obj_Crabmeat_Stop(Sst *self, Own_Crabmeat *scratch);
void Obj_Crabmeat_Projectile_Construct(Sst *self);
bool Obj_Crabmeat_Check_BelowLevel(Sst *self);
void Obj_Crabmeat(Sst *self);

#endif //_CRABMEAT_H
