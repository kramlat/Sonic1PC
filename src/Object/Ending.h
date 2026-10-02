#pragma once

#include "Object.h"

// The ending sequence (objects 87-89) and the "TRY AGAIN" / "END" screens (8B, 8C). The game modes that run them
// are in GM_Ending.c.

// Object slots the originals address by RAM label.
#define ENDING_SLOT_EMERALDS 16 // v_endemeralds: the six emeralds Sonic holds up (and the logo, v_endlogo, later)
#define ENDING_SLOT_LOGO     16
#define CREDITS_SLOT_TEXT    2  // v_credits / v_endeggman
#define CREDITS_SLOT_TRYAGAIN 3 // v_tryagain: the "TRY AGAIN" text
#define ENDING_SLOT_EGGMOBILE 24 // Eggman's wrecked Eggmobile in the background (only when it was destroyed in the Final Zone)
#define CREDITS_SLOT_CHAOS   32 // v_eggmanchaos: the emeralds Eggman juggles

void Obj_EndSonic(Object *obj);
void Obj_EndChaos(Object *obj);
void Obj_EndSTH(Object *obj);
void Obj_EndEggman(Object *obj);
void Obj_TryChaos(Object *obj);
void Obj_EndEggmobile(Object *obj);
