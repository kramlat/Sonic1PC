#include "LevelDrawCore.h"

#include "LevelData.h"

#include "Backend/VDP.h"

size_t CalcVRAMPos_2(int16_t sx, int16_t x, int16_t y) {
	uint16_t px = ((x + sx) >> 2) & ~3;
	uint16_t py = (y >> 2) & ~3;
	return (POSITIVE_MOD(py, PLANE_HEIGHT << 1) * PLANE_WIDTH) + POSITIVE_MOD(px, PLANE_WIDTH << 1);
}

size_t CalcVRAMPos(int16_t sx, int16_t sy, int16_t x, int16_t y) {
    return CalcVRAMPos_2(sx, x, y + sy);
}

size_t CalcVRAMPos_Unknown(int16_t sx, int16_t sy, int16_t x, int16_t y) {
    return CalcVRAMPos(sx, sy, x, y);
}

/**
 * Primary entry point: Calculates block data by first applying the camera's X offset.
 */
void GetBlockData(const uint8_t **meta, const uint8_t **block, int16_t sx, int16_t sy, int16_t x, int16_t y, const uint8_t *layout) {
    GetBlockData_2(meta, block, sy, sx + x, y, layout);
}

/**
 * Secondary entry point: Calculates the memory addresses for block metadata and
 * tile data based on world coordinates and the level layout.
 */
void GetBlockData_2(const uint8_t **meta, const uint8_t **block, int16_t sy, int16_t x, int16_t y, const uint8_t *layout) {
	y += sy;
	// layout points at a plane's start within the interleaved level_layout
	// buffer (LEVEL_LAYOUT_FG(0)/LEVEL_LAYOUT_BG(0)) -- rows are
	// LEVEL_LAYOUT_ROW_STRIDE (0x100) apart, not 0x80, since FG and BG share
	// each row. Chunks are 128x128 px (real Sonic 2 size), hence >>7.
	int16_t cx = (x >> 7) & (LEVEL_LAYOUT_COLS - 1);
	int16_t cy = (y >> 7) & (LEVEL_LAYOUT_ROWS - 1);
	// Real Sonic 2 uses the full byte (0-255) as the chunk ID, no reserved
	// high bit like Sonic 1 had.
	uint8_t chunk = layout[cy * LEVEL_LAYOUT_ROW_STRIDE + cx];
	if (chunk == 0) {
		*meta = level_map128;
		*block = level_map16;
		return;
	}
	uint8_t tx = (x >> 4) & 0x7;
	uint8_t ty = (y >> 4) & 0x7;
	// No -1 shift: raw layout byte N indexes Map128 table entry N directly.
	const uint8_t *metap = level_map128 + (chunk << 7) + (ty << 4) + (tx << 1);
	*meta = metap;
	size_t tile = (metap[0] << 8) | (metap[1] << 0);
	tile = tile & 0x3FF;
	*block = level_map16 + (tile << 3);
}

#define WRITE_TILE(off, xor)                                    \
    {                                                           \
        VDP_SeekVRAM(offset + (off));                           \
        uint16_t v = ((block[0] << 8) | (block[1] << 0)) ^ xor; \
        block += 2;                                             \
        VDP_WriteVRAM((const uint8_t*)&v, 2);                   \
    }

void DrawFlipXY(const uint8_t* block, size_t offset) {
	WRITE_TILE((PLANE_WIDTH << 1) + 2, 0x1800)
	WRITE_TILE((PLANE_WIDTH << 1) + 0, 0x1800)
	WRITE_TILE(                     2, 0x1800)
	WRITE_TILE(                     0, 0x1800)
}

void DrawFlipX(const uint8_t* block, size_t offset) {
	WRITE_TILE(                     2, 0x0800)
	WRITE_TILE(                     0, 0x0800)
	WRITE_TILE((PLANE_WIDTH << 1) + 2, 0x0800)
	WRITE_TILE((PLANE_WIDTH << 1) + 0, 0x0800)
}

void DrawFlipY(const uint8_t* block, size_t offset) {
	WRITE_TILE((PLANE_WIDTH << 1) + 0, 0x1000)
	WRITE_TILE((PLANE_WIDTH << 1) + 2, 0x1000)
	WRITE_TILE(                     0, 0x1000)
	WRITE_TILE(                     2, 0x1000)
}

void DrawBlock(const uint8_t *meta, const uint8_t *block, size_t offset) {
	uint8_t flag = meta[0];
	// meta[0] is the word's high byte -- META_X_FLIP (bit10 of the word) is
	// bit2 here (0x04), META_Y_FLIP (bit11) is bit3 (0x08). These used to be
	// bit3/bit4 (0x08/0x10) under the old bit format, where the tile field
	// was 11 bits instead of 10; shifted down by one when that changed, but
	// this call site never got updated to match.
	if (flag & 0x04) //X flip
		if (flag & 0x08) //Y flip
			DrawFlipXY(block, offset);
		else
			DrawFlipX(block, offset);
	else if (flag & 0x08) //Y flip
			DrawFlipY(block, offset);
	else {
		WRITE_TILE(                     0, 0x0000)
		WRITE_TILE(                     2, 0x0000)
		WRITE_TILE((PLANE_WIDTH << 1) + 0, 0x0000)
		WRITE_TILE((PLANE_WIDTH << 1) + 2, 0x0000)
	}
}

void DrawBlocks_LR_2(size_t offset, size_t pos, int16_t sx, int16_t sy, int16_t x, int16_t y, const uint8_t *layout, size_t width) {
	const uint8_t *meta;
	const uint8_t *block;
	while (width-- > 0) {
		GetBlockData(&meta, &block, sx, sy, x, y, layout);
		DrawBlock(meta, block, offset + pos);
		size_t tx = pos % (PLANE_WIDTH << 1);
		size_t ty = pos / (PLANE_WIDTH << 1);
		pos = (ty * (PLANE_WIDTH << 1)) + ((tx + 4) % (PLANE_WIDTH << 1));
		x += 16;
	}
}

void DrawBlocks_LR_3(size_t offset, size_t pos, int16_t sx, int16_t sy, int16_t x, int16_t y, const uint8_t *layout, size_t width) {
    const uint8_t* meta;
    const uint8_t* block;
    while (width-- > 0) {
        GetBlockData_2(&meta, &block, sy, sx + x, y, layout);
        DrawBlock(meta, block, offset + pos);
        size_t tx = pos & ~(PLANE_ROW_BYTES - 1);
        size_t ty = (pos + 4) & (PLANE_ROW_BYTES - 1);
        pos = tx | ty;
        x += 16;
    }
}

// A row, as the original draws it: the width of the screen and two extra columns (22 blocks at 320, from one block left of the view), no more than the plane holds. One
// block fewer leaves the block at the right edge of the view stale in every row drawn while the camera is not on a block line.
void DrawBlocks_LR(size_t offset, size_t pos, int16_t sx, int16_t sy, int16_t x, int16_t y, const uint8_t *layout) {
	size_t width = (RIGHT_EDGE_X + 32) / 16;
	if (width > PLANE_WIDTH / 2)
		width = PLANE_WIDTH / 2;
	DrawBlocks_LR_2(offset, pos, sx, sy, x, y, layout, width);
}

void DrawBlocks_TB_2(size_t offset, size_t pos, int16_t sx, int16_t sy, int16_t x, int16_t y, const uint8_t *layout, size_t height) {
	const uint8_t *meta;
	const uint8_t *block;
	while (height-- > 0) {
		GetBlockData(&meta, &block, sx, sy, x, y, layout);
		DrawBlock(meta, block, offset + pos);
		size_t tx = pos % (PLANE_WIDTH << 1);
		size_t ty = pos / (PLANE_WIDTH << 1);
		pos = (((ty + 2) % PLANE_HEIGHT) * (PLANE_WIDTH << 1)) + tx;
		y += 16;
	}
}

void DrawBlocks_TB(size_t offset, size_t pos, int16_t sx, int16_t sy, int16_t x, int16_t y, const uint8_t *layout) {
	DrawBlocks_TB_2(offset, pos, sx, sy, x, y, layout, SCROLL_ROWS);
}

// Draws the rows of blocks that fill a plane, from one block above the view, for the camera at (sx, sy)
void DrawChunks(int16_t sx, int16_t sy, const uint8_t *layout, size_t offset) {
    int16_t y = -16;
    for (size_t i = 0; i < SCROLL_ROWS; i++) {
        DrawBlocks_LR_2(offset, CalcVRAMPos(sx, sy, 0, y), sx, sy, 0, y, layout, PLANE_WIDTH / 2);
        y += 16;
    }
}
