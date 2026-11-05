#pragma once

#include "Sst.h"

typedef struct {
	int16_t size;    // pm_size, 0x32 -- trigger half-size, real data is pre-halved
	uint8_t passed;  // pm_passed, 0x34
} Own_GHZTunnel;

void Obj_GHZTunnel(Sst *self);
