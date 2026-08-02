#ifndef _SPECIALSONIC_H
#define _SPECIALSONIC_H

#include "Object.h"

//Special Stage Sonic scratch memory
typedef struct {
	uint8_t pad0[8];   //0x28-0x2F
	uint8_t hit_block; //0x30
	uint8_t pad1;      //0x31
	uint8_t *hit_addr; //0x32
} Scratch_SpecialSonic;

#endif // _SPECIALSONIC_H
