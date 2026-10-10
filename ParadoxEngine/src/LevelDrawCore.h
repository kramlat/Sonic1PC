#pragma once

#include <stdint.h>
#include <stddef.h>

#include "EngineConstants.h"
#include "Viewport.h"

// Drawing the level's blocks into a plane: the chunk layout (LevelData.h) is looked up for a position in the level, and a 16x16 block's four tiles are written to a
// plane's name table in VRAM at the matching place. `plane` is the plane written to (Viewport.h), (sx, sy) the camera, (x, y) a place relative to it. The game's scroll
// code decides what to draw when; LevelPlane.h does that for a foreground plane that follows a camera.

// Scroll dimensions (hack so that dimensions that aren't a multiple of 16 work)
#define SCROLL_WIDTH ((SCREEN_WIDTH + 15) & ~15)
// Where the column entering on the right is drawn: the screen width rounded up to whole blocks (SCROLL_WIDTH), so a partial block at the edge is covered,
// plus one more column on wide screens as a margin. The original 320 keeps exactly its own.
#define WIDE_MARGIN (SCREEN_WIDTH > 320 ? 16 : 0)
#define RIGHT_EDGE_X (SCROLL_WIDTH + WIDE_MARGIN)
#define SCROLL_HEIGHT ((SCREEN_HEIGHT + 15) & ~15)
// Rows of 16-pixel blocks drawn to fill the screen with a margin: no more than the plane holds, or the last overwrites the first
#define SCROLL_ROWS_WANTED ((SCROLL_HEIGHT + 16 + 16) / 16)
#define SCROLL_ROWS (SCROLL_ROWS_WANTED > (PLANE_HEIGHT / 2) ? (PLANE_HEIGHT / 2) : SCROLL_ROWS_WANTED)

// The tiles written to a plane whose pattern is one of `count` from `first` are taken from `bank` instead (pattern 0 of it for `first`, and so on), so that a plane can show a copy of some art of its own (the second
// view of a split screen and a background made of tiles that change with the camera: each view needs its own set). Cleared with DrawTileRemap_Clear.
void DrawTileRemap_Set(const plane_t *plane, uint16_t first, uint16_t count, const tilebank_t *bank);
void DrawTileRemap_Clear(void);

size_t CalcVRAMPos(int16_t sx, int16_t sy, int16_t x, int16_t y);
size_t CalcVRAMPos_2(int16_t sx, int16_t x, int16_t y);
size_t CalcVRAMPos_Unknown(int16_t sx, int16_t sy, int16_t x, int16_t y);
void GetBlockData(const uint8_t **meta, const uint8_t **block, int16_t sx, int16_t sy, int16_t x, int16_t y, const uint8_t *layout);
void GetBlockData_2(const uint8_t **meta, const uint8_t **block, int16_t sy, int16_t x, int16_t y, const uint8_t *layout);
void DrawFlipX(const uint8_t* block, plane_t *plane, size_t pos);
void DrawFlipY(const uint8_t* block, plane_t *plane, size_t pos);
void DrawFlipXY(const uint8_t* block, plane_t *plane, size_t pos);
void DrawBlock(const uint8_t* meta, const uint8_t* block, plane_t *plane, size_t pos);
void DrawBlocks_LR_2(plane_t *plane, size_t pos, int16_t sx, int16_t sy, int16_t x, int16_t y, const uint8_t *layout, size_t width);
void DrawBlocks_LR_3(plane_t *plane, size_t pos, int16_t sx, int16_t sy, int16_t x, int16_t y, const uint8_t *layout, size_t width);
void DrawBlocks_LR(plane_t *plane, size_t pos, int16_t sx, int16_t sy, int16_t x, int16_t y, const uint8_t *layout);
void DrawBlocks_TB_2(plane_t *plane, size_t pos, int16_t sx, int16_t sy, int16_t x, int16_t y, const uint8_t *layout, size_t height);
void DrawBlocks_TB(plane_t *plane, size_t pos, int16_t sx, int16_t sy, int16_t x, int16_t y, const uint8_t *layout);
void DrawChunks(int16_t sx, int16_t sy, const uint8_t *layout, plane_t *plane);
