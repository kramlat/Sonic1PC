#pragma once

#include <stdint.h>

//Palette cycle state
extern int16_t pcyc_num, pcyc_time;
extern uint16_t pcyc_buffer[0x18];

//Palette cycle routines
int32_t PCycle_Sega(void);
void PCycle_Title(void);
void PCycle_GHZ(void);
void PCycle_SS(void);
void PaletteCycle(void);
