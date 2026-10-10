#ifndef _SUPERSONIC_H
#define _SUPERSONIC_H

#include <stdbool.h>

// Super Sonic as the alpha has it (Sonic.c): a flag that changes his speeds, his balancing, his spin dash and his animations. The alpha's code never sets it (the transformation is not in it), so a debug build does
extern bool super_sonic_flag;

// Turns the flag on or off, with the speeds the alpha gives Super Sonic ($A00, $30, $100; $600, $C, $80 otherwise)
void SuperSonic_Set(bool on);

#endif //_SUPERSONIC_H
