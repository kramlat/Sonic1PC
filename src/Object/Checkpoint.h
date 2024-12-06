#ifndef _CHECKPOINT_H
#define _CHECKPOINT_H

#include "Object.h"

#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"

// Assets
#include "Resource/Mappings/Checkpoint.h"

// Checkpoint
typedef struct {
	uint8_t subtype;
	uint8_t pad0[6];
	struct {
		dword_s x, y;
	} pos;
	uint8_t pad1[4];
	uint16_t time;
	uint8_t pad2[7];
} Scratch_Checkpoint;

void Obj_Checkpoint_StoreInfo(Object* obj, Scratch_Checkpoint* scratch);
void Obj_Checkpoint_Construct(Object* obj, Scratch_Checkpoint* scratch);
void Obj_Checkpoint_Blue(Object* obj, Scratch_Checkpoint* scratch);
void Obj_Checkpoint_Twirl(Object* obj, Scratch_Checkpoint* scratch);
void Obj_Checkpoint(Object* obj);

#endif
