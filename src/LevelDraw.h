#pragma once

#include <stdint.h>
#include <stddef.h>
#include "Types.h"

extern const uint8_t MZ_ScrollArray[], SBZ_ScrollArray[];
extern int16_t scroll_block1_size, scroll_block2_size, scroll_block3_size, scroll_block4_size;
extern const dword_s* bg_pos_table[];

size_t CalcVRAMPos(int16_t sx, int16_t sy, int16_t x, int16_t y);
size_t CalcVRAMPos_2(int16_t sx, int16_t x, int16_t y);
size_t CalcVRAMPos_Unknown(int16_t sx, int16_t sy, int16_t x, int16_t y);
void GetBlockData(const uint8_t** meta, const uint8_t** block, int16_t sx, int16_t sy, int16_t x, int16_t y, uint8_t* layout);
void GetBlockData_2(const uint8_t** meta, const uint8_t** block, int16_t sy, int16_t x, int16_t y, uint8_t* layout);
void DrawFlipX(const uint8_t* block, size_t offset);
void DrawFlipY(const uint8_t* block, size_t offset);
void DrawFlipXY(const uint8_t* block, size_t offset);
void DrawBlock(const uint8_t* meta, const uint8_t* block, size_t offset);
void DrawChunks(int16_t sx, int16_t sy, uint8_t *layout, size_t offset);
void LoadTilesFromStart();
void DrawBGScrollBlock1(int16_t sx, int16_t sy, uint16_t *flag, uint8_t *layout, size_t offset);
void DrawBGScrollBlock2(int16_t sx, int16_t sy, uint16_t *flag, uint8_t *layout, size_t offset);
void DrawBGScrollBlock3(int16_t sx, int16_t sy, uint16_t *flag, uint8_t *layout, size_t offset);
void LoadTilesAsYouMove();
void LoadTilesAsYouMove_BGOnly();
void AnimateLevelGfx();
