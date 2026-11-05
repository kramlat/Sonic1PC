#ifndef _CHECKPOINT_H
#define _CHECKPOINT_H

#include "Sst.h"

#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"

// Assets

// Checkpoint
typedef struct {
	struct {
		dword_s x, y;
	} pos;
	uint16_t time;
} Own_Checkpoint;

void Obj_Checkpoint_StoreInfo(Sst *self, const Own_Checkpoint *scratch);
void Obj_Checkpoint(Sst *self);

#endif
