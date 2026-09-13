#pragma once

#include "Object.h"

typedef struct {
	uint8_t subtype; // 0x28 -- see obSubtype bit layout in the real source
	uint8_t pad0[9]; // 0x29-0x31
	int16_t size;    // pm_size, 0x32 -- trigger half-size, real data is pre-halved
	uint8_t passed;  // pm_passed, 0x34
} Scratch_GHZTunnel;

void Obj_GHZTunnel(Object *obj);
