#pragma once

#include <stdbool.h>
#include <stdint.h>

//Palette cycle state
extern int16_t pcyc_num, pcyc_time;
extern uint16_t pcyc_buffer[0x18];
extern bool f_conveyrev;

//Palette cycle routines
int32_t PCycle_Sega(void);
void PCycle_Title(void);
void PCycle_GHZ(void);
void PCycle_SS(void);
void PCycle_MZ(void);
void PCycle_LZ(void);
void PCycle_SLZ(void);
void PCycle_SYZ(void);
void PCycle_SBZ(void);
void PaletteCycle(void);
