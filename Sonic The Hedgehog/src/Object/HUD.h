#ifndef _OBJECT_HUD_H
#define _OBJECT_HUD_H

#include "Sst.h"


extern bool hud_lives_lower_left; // the lives counter of a wide picture goes in the lower left (a split screen)

extern bool hud_visible; // a level or the ending is on: HUD_Draw draws the HUD
void HUD_Draw(void);     // queues the HUD's sprites (it is not an object: no slot, no id)

#endif //_OBJECT_HUD_H
