#pragma once

#include "Object.h"

// The continue screen's objects: 80 (the text, floor light and the mini Sonics counting the continues you have left) and
// 81 (Sonic, who falls in, waits and runs off when Start is pressed). The game mode is in GM_Continue.c.

// Object slots the original addresses by RAM label (Sonic himself is slot 0).
#define CONTINUE_SLOT_TEXT  1
#define CONTINUE_SLOT_LIGHT 2
#define CONTINUE_SLOT_ICON  3 // the first of the mini Sonics

void Obj_ContScrItem(Object *obj);
void Obj_ContSonic(Object *obj);
