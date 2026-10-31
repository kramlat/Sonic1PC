#ifndef _ANIMAL_H
#define _ANIMAL_H

#include "Sst.h"
#include "MathUtil.h"
#include "Level.h"
#include "Video.h"
#include <stddef.h>
#include <stdint.h>
#include "LevelCollision.h"
#include "Macros.h"

//Animal Assets
#ifndef Animals_Build
extern const uint8_t Mappings_Animals1[];
extern const uint8_t Mappings_Animals2[];
extern const uint8_t Mappings_Animals3[];
#endif

// Animal Variables
typedef struct {
    int16_t xsp;
    int16_t ysp;
    const void* mappings;
}  animalvar_t;

extern const uint8_t AnimalVarIndex[][2];
extern const animalvar_t AnimalVariables[];

// Offsets must match the original object RAM layout: the level loader
// writes the subtype to 0x28 (scratch.u8[0]) and Obj_Explosion_Animal
// copies points into 0x3E (scratch.u16[0xB]). A previous layout had
// everything shifted by 2 bytes, so subtype was read from 0x29 -- any stale
// byte there sent prison animals down the ending-sequence path.
typedef struct {
	uint8_t subtype; //0x28
	uint8_t reverse; //0x29
	uint8_t pad1[6];
	uint8_t routine; //0x30
	uint8_t pad2[1];
	int16_t xsp;     //0x32
	int16_t ysp;     //0x34
	uint16_t timer;  //0x36
	uint8_t pad3[6];
	uint16_t points; //0x3E
} Own_Animals;

STATIC_ASSERT(offsetof(Own_Animals, routine) == 0x30 - 0x28, "Own_Animals.routine must be at 0x30");
STATIC_ASSERT(offsetof(Own_Animals, timer) == 0x36 - 0x28, "Own_Animals.timer must be at 0x36");
STATIC_ASSERT(offsetof(Own_Animals, points) == 0x3E - 0x28, "Own_Animals.points must be at 0x3E");

void Obj_Animals_Construct(Sst *self, Own_Animals *scratch);
void Obj_Animals_FromEnemy(Sst *self, Own_Animals *scratch);
void Obj_Animals_Main(Sst *self, const Own_Animals *scratch);
void Obj_Animals_Walk(Sst *self, const Own_Animals *scratch);
void Obj_Animals_Fly(Sst *self, const Own_Animals *scratch);
void Obj_Animals_Prison(Sst *self, Own_Animals *scratch);
bool Obj_Animals_CheckInRange(Sst *self);
void Obj_Animals_FlickyWait(Sst *self, const Own_Animals *scratch);
void Obj_Animals_UpdateRender(Sst *self);
void Obj_Animals_UpdateFrame(Sst *self, Own_Animals *scratch);
void Obj_Animals_FlickyJump(Sst *self, Own_Animals *scratch);
void Obj_Animals_RabbitWait(Sst *self, const Own_Animals *scratch);
void Obj_Animals_LandJump(Sst *self, Own_Animals *scratch);
void Obj_Animals_SingleBounce(Sst *self, const Own_Animals *scratch);
void Obj_Animals_FlyBounce(Sst *self, Own_Animals *scratch);
void Obj_Animals_DoubleBounce(Sst *self, Own_Animals *scratch);
void Obj_Animals(Sst *self);

#endif
