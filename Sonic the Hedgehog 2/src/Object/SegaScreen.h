#ifndef _SEGASCREEN_H
#define _SEGASCREEN_H

#include <stdbool.h>

#include "Sst.h"

void Obj_SegaSonic(Sst *self);            // Sonic running across the logo (the pre-beta's $B0)
void Obj_SegaTrademarkHider(Sst *self);   // the blocks over the trademark ($B1)
extern bool sega_animation_done;          // the second sweep is over: the voice is next

#endif //_SEGASCREEN_H
