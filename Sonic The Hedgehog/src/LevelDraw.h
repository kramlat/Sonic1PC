#pragma once

#include <stdint.h>
#include <stddef.h>
#include "Types.h"
#include "LevelDrawCore.h"

extern const uint8_t MZ_ScrollArray[], SBZ_ScrollArray[];
extern int16_t scroll_block1_size, scroll_block2_size, scroll_block3_size, scroll_block4_size;
extern const dword_s* bg_pos_table[];
extern const dword_s* bg_pos_table_dup[];
extern const dword_s* bg_pos_table_y[];
extern const dword_s* bg_pos_table_y_dup[];

void LoadTilesFromStart(void);
void DrawBG_Top(int16_t sx, int16_t sy, uint16_t *flag, const uint8_t *layout, size_t offset);
void DrawBG_Bottom(int16_t sx, int16_t sy, uint16_t *flag, const uint8_t *layout, size_t offset);
void DrawBG_Block3(int16_t sx, int16_t sy, uint16_t *flag, const uint8_t *layout, size_t offset);
void LoadTilesAsYouMove(void);
void LoadTilesAsYouMove_BGOnly(void);
void AnimateLevelGfx(void);

// Decompresses the ending sequence's extra flower art (the second sunflower wall and flowers 3 and 4); GM_Ending calls it.
void Ending_LoadFlowerArt(void);
