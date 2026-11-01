#ifndef _SEGASCREEN_H
#define _SEGASCREEN_H

#include <stdbool.h>

#include "Sst.h"
#include "EngineConstants.h"

// The screen's planes: 128 tiles wide and 32 high, as the pre-beta's are (the blocks' strips are longer than a 64 wide plane)
#define SEGA_PLANE_WIDTH  128
#define SEGA_PLANE_HEIGHT 32
static inline int SegaPlaneRows(void) { return (SCREEN_TALLADD + 8) / 16; } // (the rows a taller picture has above the 28 of the original: where the plane starts)

void Obj_SegaSonic(Sst *self);            // Sonic running across the logo (the pre-beta's $B0)
void Obj_SegaTrademarkHider(Sst *self);   // the blocks over the trademark ($B1)
extern bool sega_animation_done;          // the second sweep is over: the voice is next

#endif //_SEGASCREEN_H
