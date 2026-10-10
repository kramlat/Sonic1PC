#ifndef _OBJECT_HUD_H
#define _OBJECT_HUD_H

#include "Object.h"

// HUD assets
#include "Resource/Mappings/HUD.h"

extern uint8_t hud_lives_slot; // the object slot the lives counter of a wide picture takes (a game that has something in the default one picks another)
extern bool hud_lives_lower_left; // the lives counter of a wide picture goes in the lower left (a split screen)

#endif //_OBJECT_HUD_H
