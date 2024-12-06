#ifndef _ANIMAL_H
#define _ANIMAL_H

#include "Object.h"
#include "MathUtil.h"
#include "Level.h"
#include "Video.h"
#include <stdint.h>
#include "LevelCollision.h"

//Animal Assets
#include "Resource/Mappings/Animals1.h"
#include "Resource/Mappings/Animals2.h"
#include "Resource/Mappings/Animals3.h"

// Animal Variables
typedef struct {
    int16_t xsp;
    int16_t ysp;
    const void* mappings;
}  animalvar_t;

typedef struct {
	uint8_t pad1[1];
	uint8_t subtype; //0x28
	uint8_t reverse; //0x29
	uint8_t pad2[3];
	uint8_t routine; //0x30
	int16_t xsp;     //0x32
	int16_t ysp;     //0x34
	uint16_t timer;  //0x36
	uint8_t pad3[0x8];
	uint16_t points; //0x3E
} Scratch_Animals;

void Obj_Animals_Construct(Object* obj, Scratch_Animals* scratch);
void Obj_Animals_FromEnemy(Object* obj, Scratch_Animals* scratch);
void Obj_Animals_Main(Object* obj, Scratch_Animals* scratch);
void Obj_Animals_Walk(Object* obj, Scratch_Animals* scratch);
void Obj_Animals_Fly(Object* obj, Scratch_Animals* scratch);
void Obj_Animals_Prison(Object* obj, Scratch_Animals* scratch);
bool Obj_Animals_CheckInRange(Object* obj);
void Obj_Animals_FlickyWait(Object* obj, Scratch_Animals* scratch);
void Obj_Animals_UpdateRender(Object* obj);
void Obj_Animals_UpdateFrame(Object* obj, Scratch_Animals* scratch);
void Obj_Animals_FlickyJump(Object* obj, Scratch_Animals* scratch);
void Obj_Animals_RabbitWait(Object* obj, Scratch_Animals* scratch);
void Obj_Animals_LandJump(Object* obj, Scratch_Animals* scratch);
void Obj_Animals_SingleBounce(Object* obj, Scratch_Animals* scratch);
void Obj_Animals_FlyBounce(Object *obj, Scratch_Animals* scratch);
void Obj_Animals_DoubleBounce(Object* obj, Scratch_Animals* scratch);
void Obj_Animals(Object* obj);

#endif
