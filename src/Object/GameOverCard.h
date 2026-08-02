#ifndef _GAMEOVER_CARD_H
#define _GAMEOVER_CARD_H

#include "Object.h"

// Game Over card assets
#include "Resource/Mappings/GameOver.h"

// Game Over card constants
#define TO_ADD SCREEN_WIDEADD2
#define FROM_ADD (TO_ADD + ((SCREEN_WIDEADD2 + 0xF) & ~0xF))
#define FROM_SUB ((0x10 - TO_ADD) & 0xF)

void Obj_GameOverCard(Object* obj);

#endif //_GAMEOVER_CARD_H
