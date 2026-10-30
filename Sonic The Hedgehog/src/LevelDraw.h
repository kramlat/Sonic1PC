#pragma once

#include <stdint.h>
#include <stddef.h>
#include "Types.h"
#include "LevelDrawCore.h"

extern const uint8_t MZ_ScrollArray[], SBZ_ScrollArray[];
extern int16_t scroll_block1_size, scroll_block2_size, scroll_block3_size, scroll_block4_size;

void LoadTilesFromStart(void);
void LoadTilesAsYouMove(void);
void LoadTilesAsYouMove_BGOnly(void);
void AnimateLevelGfx(void);

// Decompresses the ending sequence's extra flower art (the second sunflower wall and flowers 3 and 4); GM_Ending calls it.
void Ending_LoadFlowerArt(void);
