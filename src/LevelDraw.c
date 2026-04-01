#include "LevelDraw.h"

#include "Constants.h"
#include "Level.h"
#include "LevelScroll.h"

#include "Backend/VDP.h"

// Scroll dimensions (hack so that dimensions that aren't a multiple of 16 work)
#define SCROLL_WIDTH ((SCREEN_WIDTH + 15) & ~15)
#define SCROLL_HEIGHT ((SCREEN_HEIGHT + 15) & ~15)

// Scroll blocks
int16_t scroll_block1_size, scroll_block2_size, scroll_block3_size, scroll_block4_size;

const uint8_t MZ_ScrollArray[128] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06
    };

const uint8_t SBZ_ScrollArray[32] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06
    };

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
void GetBlockData(const uint8_t** meta, const uint8_t** block, int16_t sx, int16_t sy, int16_t x, int16_t y, uint8_t* layout) {
    GetBlockData_2(meta, block, sy, sx + x, y, layout);
}

/**
 * Secondary entry point: Calculates the memory addresses for block metadata and
 * tile data based on world coordinates and the level layout.
 */
void GetBlockData_2(const uint8_t **meta, const uint8_t **block, int16_t sy, int16_t x, int16_t y, uint8_t *layout) {
	y += sy;
	int16_t cx = (x >> 8) & 0x3F;
	int16_t cy = (y >> 8) & 0x7;
	uint8_t chunk = layout[(cy << 7) + cx] & 0x7F;
	if (chunk == 0) {
		*meta = level_map256;
		*block = level_map16;
		return;
	}
	uint8_t tx = (x >> 4) & 0xF;
	uint8_t ty = (y >> 4) & 0xF;
	const uint8_t *metap = (level_map256 - 0x200) + (chunk << 9) + (ty << 5) + (tx << 1);
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
	if (flag & 0x08) //X flip
		if (flag & 0x10) //Y flip
			DrawFlipXY(block, offset);
		else
			DrawFlipX(block, offset);
	else if (flag & 0x10) //Y flip
			DrawFlipY(block, offset);
	else {
		WRITE_TILE(                     0, 0x0000)
		WRITE_TILE(                     2, 0x0000)
		WRITE_TILE((PLANE_WIDTH << 1) + 0, 0x0000)
		WRITE_TILE((PLANE_WIDTH << 1) + 2, 0x0000)
	}
}

void DrawBlocks_LR_2(size_t offset, size_t pos, int16_t sx, int16_t sy, int16_t x, int16_t y, uint8_t *layout, size_t width) {
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

#ifdef SCP_REV01
void DrawBlocks_LR_3(size_t offset, size_t pos, int16_t sx, int16_t sy, int16_t x, int16_t y, uint8_t* layout, size_t width) {
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
#endif

void DrawBlocks_LR(size_t offset, size_t pos, int16_t sx, int16_t sy, int16_t x, int16_t y, uint8_t *layout) {
	DrawBlocks_LR_2(offset, pos, sx, sy, x, y, layout, (SCROLL_WIDTH + 16 + 16) / 16);
}

void DrawBlocks_TB_2(size_t offset, size_t pos, int16_t sx, int16_t sy, int16_t x, int16_t y, uint8_t *layout, size_t height) {
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

void DrawBlocks_TB(size_t offset, size_t pos, int16_t sx, int16_t sy, int16_t x, int16_t y, uint8_t *layout) {
	DrawBlocks_TB_2(offset, pos, sx, sy, x, y, layout, (SCROLL_HEIGHT + 16 + 16) / 16);
}

const dword_s* bg_pos_table[] = {
    &bg_scrpos_x,  // Index 0
    &bg_scrpos_x,  // Index 2
    &bg2_scrpos_x, // Index 4
    &bg3_scrpos_y  // Index 6 (Kept .y as per existing prototyping, though ASM used .x)
};

void DrawBlocks_BG(size_t offset, int16_t sx, int16_t sy, int16_t y, uint8_t *layout, const uint8_t *array) {
	uint8_t bg_pos_i = array[y >> 4];
	if (bg_pos_i != 0) {
		sx = bg_pos_table[bg_pos_i >> 1]->f.u;
		sy = (sy & ~0xF) % SCROLL_HEIGHT;
		DrawBlocks_LR(offset, CalcVRAMPos(sx, sy, 0, y), sx, sy, 0, y, layout);
	}
	else
		DrawBlocks_LR_2(offset, CalcVRAMPos(sx, sy, 0, y), sx, sy, 0, y, layout, PLANE_WIDTH);
}

void Draw_GHZ_Bg(int16_t sy, uint8_t *layout, size_t offset) {
	int16_t y = 0;
	for (size_t i = 0; i < (SCROLL_HEIGHT + 16 + 16) / 16; i++) {
		static const uint8_t bg_array[] = {0x00, 0x00, 0x00, 0x00, 0x06, 0x06, 0x06, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
		DrawBlocks_BG(offset, bg_scrpos_y.f.u, sy, y, layout, bg_array);
		y += 16;
	}
}

void Draw_MZ_Bg(int16_t sy, uint8_t* layout, size_t offset) {
    int16_t y = 0;
    for (size_t i = 0; i < (SCREEN_HEIGHT + 16 + 16) / 16; i++) {
        static const uint8_t bg_array[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
        0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06
    };
        DrawBlocks_BG(offset, bg_scrpos_x.f.u, sy, y, layout, bg_array);
        y += 16;
    }
}

void Draw_SBZ_Bg(int16_t sy, uint8_t* layout, size_t offset) {
    int16_t y = 0;
    for (size_t i = 0; i < (SCREEN_HEIGHT + 16 + 16) / 16; i++) {
        static const uint8_t bg_array[] = {
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06
    };
       DrawBlocks_BG(offset, bg_scrpos_x.f.u, sy, y, layout, bg_array);
        y += 16;
    }
}

// Level drawing functions
void DrawChunks(int16_t sx, int16_t sy, uint8_t* layout, size_t offset) {
    int16_t y = -16;
    for (size_t i = 0; i < (SCROLL_HEIGHT + 16 + 16) / 16; i++) {
        DrawBlocks_LR_2(offset, CalcVRAMPos(sx, sy, 0, y), sx, sy, 0, y, layout, PLANE_WIDTH / 2);
        y += 16;
    }
}

void LoadTilesFromStart() {
    DrawChunks(scrpos_x.f.u, scrpos_y.f.u, level_layout[0][0], VRAM_FG);
#ifndef SCP_REV00
    if (LEVEL_ZONE(level_id) == ZoneId_GHZ || LEVEL_ZONE(level_id) == ZoneId_EndZ)
        Draw_GHZ_Bg(bg_scrpos_y.f.u, level_layout[0][1], VRAM_BG);
    else if (LEVEL_ZONE(level_id) == ZoneId_MZ)
        Draw_MZ_Bg(bg_scrpos_y.f.u, level_layout[0][1], VRAM_BG);
    else if (level_id == ZoneId_SBZ << 8)
        Draw_SBZ_Bg(bg_scrpos_y.f.u, level_layout[0][1], VRAM_BG);
    else
#endif
    DrawChunks(bg_scrpos_x.f.u, bg_scrpos_y.f.u, level_layout[0][1], VRAM_BG);
}

void DrawBGScrollBlock1(int16_t sx, int16_t sy, uint16_t *flag, uint8_t *layout, size_t offset) {
	//Check if any flags have been set
	if (*flag == 0)
		return;
#ifdef SCP_REV00
    if (*flag & SCROLL_FLAG_UP) {
        *flag &= ~SCROLL_FLAG_UP;
        size_t pos = CalcVRAMPos(sx, sy, -16, -16);
        DrawBlocks_LR_2(offset, pos, sx, sy, -16, -16, layout, (512 / 16));
    }
    if (*flag & SCROLL_FLAG_DOWN) {
        *flag &= ~SCROLL_FLAG_DOWN;
        size_t pos = CalcVRAMPos(sx, sy, -16, SCREEN_HEIGHT);
        DrawBlocks_LR_2(offset, pos, sx, sy, -16, SCREEN_HEIGHT, layout, (512 / 16));
    }
    if (*flag & SCROLL_FLAG_LEFT) {
        *flag &= ~SCROLL_FLAG_LEFT;
        int16_t size1 = scroll_block1_size - (sy & -16);
        if (size1 >= 0) {
            size1 >>= 4; // Convert to blocks
            if (size1 > ((SCREEN_HEIGHT + 16 + 16) / 16) - 1)
                size1 = ((SCREEN_HEIGHT + 16 + 16) / 16) - 1;
            size_t pos = CalcVRAMPos(sx, sy, -16, -16);
            DrawBlocks_TB_2(offset, pos, sx, sy, -16, -16, layout, size);
        }
    }
    if (*flag & SCROLL_FLAG_RIGHT) {
        *flag &= ~SCROLL_FLAG_RIGHT;
        int16_t size = v_scroll_block_1_size - (sy & -16);
        if (size >= 0) {
            size >>= 4;
            if (size > ((SCREEN_HEIGHT + 16 + 16) / 16) - 1)
                size = ((SCREEN_HEIGHT + 16 + 16) / 16) - 1;
            size_t pos = CalcVRAMPos(sx, sy, SCREEN_WIDTH, -16);
            DrawBlocks_TB_2(offset, pos, sx, sy, SCREEN_WIDTH, -16, layout, size);
        }
    }
#else
	if (*flag & SCROLL_FLAG_UP) {
		DrawBlocks_LR_2(offset, CalcVRAMPos(sx, sy, -16, -16), sx, sy, -16, -16, layout, PLANE_WIDTH / 2);
		*flag &= ~SCROLL_FLAG_UP;
	}
	if (*flag & SCROLL_FLAG_DOWN) {
		DrawBlocks_LR_2(offset, CalcVRAMPos(sx, sy, -16, SCROLL_HEIGHT), sx, sy, -16, SCROLL_HEIGHT, layout, PLANE_WIDTH / 2);
		*flag &= ~SCROLL_FLAG_DOWN;
	}
	if (*flag & SCROLL_FLAG_LEFT) {
		DrawBlocks_TB(offset, CalcVRAMPos(sx, sy, -16, -16), sx, sy, -16, -16, layout);
		*flag &= ~SCROLL_FLAG_LEFT;
	}
	if (*flag & SCROLL_FLAG_RIGHT) {
		DrawBlocks_TB(offset, CalcVRAMPos(sx, sy, SCROLL_WIDTH, -16), sx, sy, SCROLL_WIDTH, -16, layout);
		*flag &= ~SCROLL_FLAG_RIGHT;
	}
	if (*flag & SCROLL_FLAG_UP2) {
		DrawBlocks_LR_3(offset, CalcVRAMPos(0, sy, 0, -16), 0, sy, 0, -16, layout, PLANE_WIDTH / 2);
		*flag &= ~SCROLL_FLAG_UP2;
	}
	if (*flag & SCROLL_FLAG_DOWN2) {
		DrawBlocks_LR_3(offset, CalcVRAMPos(0, sy, 0, SCROLL_HEIGHT), 0, sy, 0, SCROLL_HEIGHT, layout, PLANE_WIDTH / 2);
		*flag &= ~SCROLL_FLAG_DOWN2;
	}
#endif
}


void DrawBGScrollBlock2(int16_t sx, int16_t sy, uint16_t *flag, uint8_t *layout, size_t offset) {
	if (*flag == 0)
		return;
	#if SCP_REV00
		if (*flag & 0x04) {
			*flag &= ~0x04;
			if (sx >= 16) {
				int16_t current_sy = sy & -16;
				int16_t remaining_coverage = scroll_block1_size - current_sy;
				int16_t row_count = (remaining_coverage >> 4) - 14;
				if (row_count < 0) {
					size_t vram_pos = CalcVRAMPos(sx, sy, -16, remaining_coverage);
					DrawBlocks_TB_2(offset, vram_pos, sx, sy, -16, remaining_coverage, layout, -row_count);
				}
			}
		}
		if (*flag & 0x08) {
			*flag &= ~0x08;
			int16_t current_sy = sy & -16;
			int16_t remaining_coverage = scroll_block1_size - current_sy;
			int16_t row_count = (remaining_coverage >> 4) - 14;
			if (row_count < 0) {
				size_t vram_pos = CalcVRAMPos(sx, sy, SCREEN_WIDTH, remaining_coverage);
				DrawBlocks_TB_2(offset, vram_pos, sx, sy, SCREEN_WIDTH, remaining_coverage, layout, -row_count);
			}
		}
	#else
		if (LEVEL_ZONE(level_id) != ZoneId_SBZ) {
			if (*flag & SCROLL_FLAG_LEFT2) {
				DrawBlocks_TB_2(offset, CalcVRAMPos(sx, sy, -16, SCROLL_HEIGHT / 2), sx, sy, -16, SCROLL_HEIGHT / 2, layout, 3);
				*flag &= ~SCROLL_FLAG_LEFT2;
			}
			if (*flag & SCROLL_FLAG_RIGHT2) {
				DrawBlocks_TB_2(offset, CalcVRAMPos(sx, sy, SCROLL_WIDTH, SCROLL_HEIGHT / 2), sx, sy, SCROLL_WIDTH, SCROLL_HEIGHT / 2, layout, 3);
				*flag &= ~SCROLL_FLAG_RIGHT2;
			}
		} else {
			int16_t vertical_offset = -16; // Margin for drawing new tiles
			if (*flag & 0x01) {
				*flag &= ~0x01;
				DrawBlocks_BG(offset, sx, bg_scrpos_y.f.u, vertical_offset, layout, SBZ_ScrollArray);
			} else if (*flag & 0x02) {
				*flag &= ~0x02;
				vertical_offset = SCREEN_HEIGHT; // Draw slice at the bottom of the screen
				DrawBlocks_BG(offset, sx, bg_scrpos_y.f.u, vertical_offset, layout, SBZ_ScrollArray);
			}
			uint8_t sync_bits = (*flag & 0xA8);
			if (sync_bits != 0) {
				*flag= (uint16_t)(sync_bits >> 1);
				int16_t sync_y_off = -16;
				DrawBlocks_BG(offset, sx, bg_scrpos_y.f.u, sync_y_off, layout, SBZ_ScrollArray);
			}
			return;
		}
	#endif
}

void DrawBGScrollBlock3(int16_t sx, int16_t sy, uint16_t *flag, uint8_t *layout, size_t offset)
{
	//Check if any flags have been set
	if (*flag == 0)
		return;
	#ifdef SCP_REV00
		if (*flag & SCROLL_FLAG_LEFT2) {
			*flag &= ~SCROLL_FLAG_LEFT2;
			int16_t dy = (SCREEN_HEIGHT - 16) - (sy & -16);
			DrawBlocks_TB_2(offset, CalcVRAMPos_Unknown(sx, sy, -16, dy), sx, sy, -16, dy, layout, 3);
		}
		if (*flag & SCROLL_FLAG_RIGHT2) {
			*flag &= ~SCROLL_FLAG_RIGHT2;
			int16_t dy = (SCREEN_HEIGHT - 16) - (sy & -16);
			DrawBlocks_TB_2(offset, CalcVRAMPos_Unknown(sx, sy, SCROLL_WIDTH, dy), sx, sy, SCROLL_WIDTH, dy, layout, 3);
		}
	#else
		//Run completely different code if in Marble Zone (what)
		if (LEVEL_ZONE(level_id) != ZoneId_MZ) {
			if (*flag & SCROLL_FLAG_LEFT2) {
				DrawBlocks_TB_2(offset, CalcVRAMPos(sx, sy, -16, 64), sx, sy, -16, 64, layout, 3);
				*flag &= ~SCROLL_FLAG_LEFT2;
			}
			if (*flag & SCROLL_FLAG_RIGHT2) {
				DrawBlocks_TB_2(offset, CalcVRAMPos(sx, sy, SCROLL_WIDTH, 64), sx, sy, SCROLL_WIDTH, 64, layout, 3);
				*flag &= ~SCROLL_FLAG_RIGHT2;
			}
		} else {
			int16_t y_rel = -16;
			if (!(*flag & SCROLL_FLAG_LEFT)) {
				if (*flag & SCROLL_FLAG_RIGHT) {
					*flag &= ~SCROLL_FLAG_RIGHT;
					y_rel = SCREEN_HEIGHT;
				} else goto check_mz_col;
			} else *flag &= ~SCROLL_FLAG_LEFT;
			DrawBlocks_BG(offset, sx, bg_scrpos_y_dup.f.u - 0x200, y_rel, layout, MZ_ScrollArray);
			check_mz_col:
			if ((*flag & 0xFF) == 0) return;
			int16_t dy_c = -16;
			uint8_t cf = (uint8_t)(*flag & 0xFF);
			if (cf & 0xA8) {
				cf >>= 1;
				*flag = (*flag & 0xFF00) | cf;
				dy_c = SCROLL_WIDTH;
			}
			int16_t dx_l = -16;
			int16_t y_b = (bg_scrpos_y_dup.f.u - 0x200) & 0x7F0;
			const uint8_t* m_ptr = &MZ_ScrollArray[y_b >> 4];
			for (int i = 0; i < 16; i++) {
				uint8_t bit = *m_ptr++;
				if (*flag & (1 << bit)) {
					const uint8_t *m, *b;
					const dword_s* row_sx_ptr = bg_pos_table[bit >> 1];
					int16_t rsx = row_sx_ptr ? row_sx_ptr->f.u : sx;
					GetBlockData(&m, &b, rsx, bg_scrpos_y_dup.f.u, dx_l, dy_c, layout);
					DrawBlock(m, b, CalcVRAMPos(rsx, bg_scrpos_y_dup.f.u, dx_l, dy_c));
				}
				dx_l += 16;
			}
			*flag &= 0xFF00;
		}
	#endif
}

void LoadTilesAsYouMove() {
    DrawBGScrollBlock1(bg_scrpos_x_dup.f.u, bg_scrpos_y_dup.f.u,
                       &bg1_scroll_flags_dup, level_layout[0][1], VRAM_BG);

    DrawBGScrollBlock2(bg2_scrpos_x_dup.f.u, bg2_scrpos_y_dup.f.u,
                       &bg2_scroll_flags_dup, level_layout[0][1], VRAM_BG);

#ifdef SCP_REV01
    // REV01 added a third scroll block call
    DrawBGScrollBlock3(bg3_scrpos_x_dup.f.u, bg3_scrpos_y_dup.f.u,
                       &bg3_scroll_flags_dup, level_layout[0][1], VRAM_BG);
#endif
    if (fg_scroll_flags_dup == 0)
        return;
    int16_t sx = scrpos_x_dup.f.u;
    int16_t sy = scrpos_y_dup.f.u;
    uint8_t* layout = level_layout[0][0];
    if (fg_scroll_flags_dup & SCROLL_FLAG_UP) {
         size_t pos = CalcVRAMPos(sx, sy, -16, -16);
        DrawBlocks_LR(VRAM_FG, pos, sx, sy, -16, -16, layout);
        fg_scroll_flags_dup &= ~SCROLL_FLAG_UP;
    }
    if (fg_scroll_flags_dup & SCROLL_FLAG_DOWN) {
        size_t pos = CalcVRAMPos(sx, sy, -16, SCREEN_HEIGHT);
        DrawBlocks_LR(VRAM_FG, pos, sx, sy, -16, SCREEN_HEIGHT, layout);
        fg_scroll_flags_dup &= ~SCROLL_FLAG_DOWN;
    }

     if (fg_scroll_flags_dup & SCROLL_FLAG_LEFT) {
         size_t pos = CalcVRAMPos(sx, sy, -16, -16);
        DrawBlocks_TB(VRAM_FG, pos, sx, sy, -16, -16, layout);
        fg_scroll_flags_dup &= ~SCROLL_FLAG_LEFT;
    }

    if (fg_scroll_flags_dup & SCROLL_FLAG_RIGHT) {
        size_t pos = CalcVRAMPos(sx, sy, SCREEN_WIDTH, -16);
        DrawBlocks_TB(VRAM_FG, pos, sx, sy, SCREEN_WIDTH, -16, layout);
        fg_scroll_flags_dup &= ~SCROLL_FLAG_RIGHT;
    }
}

void LoadTilesAsYouMove_BGOnly() {
    DrawBGScrollBlock1(bg_scrpos_x.f.u, bg_scrpos_y.f.u, &bg1_scroll_flags, level_layout[0][1], VRAM_BG);
    DrawBGScrollBlock2(bg2_scrpos_x.f.u, bg2_scrpos_y.f.u, &bg2_scroll_flags, level_layout[0][1], VRAM_BG);
    // No scroll block 3, even in REV01... odd
}

void LoadTiles(const uint8_t *source, uint16_t count) {
	do {
		VDP_WriteVRAM(source, TILE_SIZE);
		source += TILE_SIZE;
	} while (count-- != 0);
}

// Level art animation
#include "Resource/Art/GHZFlowerLarge.h"
#include "Resource/Art/GHZFlowerSmall.h"
#include "Resource/Art/GHZWaterfall.h"
#include "Resource/Art/BigRing.h"

#define ArtTile_Giant_Ring 0x400

static void AniArt_GiantRing() {
    const uint16_t size = 14;

    if (gfx_big_ring == 0) {
        return;
    }

    gfx_big_ring -= size * 32;

    VDP_SeekVRAM((ArtTile_Giant_Ring * 32) + gfx_big_ring);
    VDP_WriteVRAM(Art_BigRing + gfx_big_ring, size * 0x20);
}

// Animate waterfall
void AniArt_GHZWaterfall() {
    if (--level_anim[0].time < 0) {
        // Increment frame and reset timer
        level_anim[0].time = 5;
        uint8_t frame = level_anim[0].frame++ & 1;

        // Write to VRAM
        VDP_SeekVRAM(0x6F00);
        VDP_WriteVRAM(Art_GHZWaterfall + (frame * 8 * 0x20), 8 * 0x20);
    }
}

// Animate large flowers
void AniArt_GHZFlowerLarge() {
    if (--level_anim[1].time < 0) {
        // Increment frame and reset timer
        level_anim[1].time = 15;
        uint8_t frame = level_anim[1].frame++ & 1;

        // Write to VRAM
        VDP_SeekVRAM(0x6B80);
        VDP_WriteVRAM(Art_GHZFlowerLarge + (frame * 16 * 0x20), 16 * 0x20);
    }
}

// Animate small flowers
void AniArt_GHZFlowerSmall() {
    if (--level_anim[2].time < 0) {

        // Increment frame and reset timer
        level_anim[2].time = 7;

        static const uint8_t seq[4] = { 0, 1, 2, 1 };
        uint8_t frame = seq[level_anim[2].frame++ & 3];
        if (!(frame & 1))
            level_anim[2].time = 127;

        // Write to VRAM
        VDP_SeekVRAM(0x6D80);
        VDP_WriteVRAM(Art_GHZFlowerSmall + (frame * 12 * 0x20), 12 * 0x20);
    }
}

// Level Animation Index Mappings for Marble Zone
#define LAVA_ANIM    0 // uses time and frame
#define MAGMA_ANIM   1 // uses time (frame was for prototype UFO)
#define TORCH_TIMER  2 // uses time (v_lani2_time)
#define TORCH_ANIM   3 // uses frame (v_lani3_frame)
typedef void (*MagmaShiftFunc)(const uint8_t*, uint16_t);

#include "Resource/Art/MZLava1.h"
#include "Resource/Art/MZLava2.h"
#include "Resource/Art/MZTorch.h"

#define ArtTile_MZ_Animated_Magma 0x2E2
#define ArtTile_MZ_Animated_Lava 0x2D2
#define ArtTile_MZ_Torch 0x2F2


void MagmaRow_Shift0(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift1(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[1] << 24) | (src[2] << 16) | (src[3] << 8) | src[4]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift2(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[2] << 24) | (src[3] << 16) | (src[4] << 8) | src[5]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift3(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[3] << 24) | (src[4] << 16) | (src[5] << 8) | src[6]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift4(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[4] << 24) | (src[5] << 16) | (src[6] << 8) | src[7]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift5(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[5] << 24) | (src[6] << 16) | (src[7] << 8) | src[8]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift6(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[6] << 24) | (src[7] << 16) | (src[8] << 8) | src[9]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift7(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[7] << 24) | (src[8] << 16) | (src[9] << 8) | src[10]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift8(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[8] << 24) | (src[9] << 16) | (src[10] << 8) | src[11]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift9(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[9] << 24) | (src[10] << 16) | (src[11] << 8) | src[12]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift10(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[10] << 24) | (src[11] << 16) | (src[12] << 8) | src[13]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift11(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[11] << 24) | (src[12] << 16) | (src[13] << 8) | src[14]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift12(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[12] << 24) | (src[13] << 16) | (src[14] << 8) | src[15]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift13(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[13] << 24) | (src[14] << 16) | (src[15] << 8) | src[0]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift14(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[14] << 24) | (src[15] << 16) | (src[0] << 8) | src[1]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift15(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[15] << 24) | (src[0] << 16) | (src[1] << 8) | src[2]);
		src += 16;
	} while (lines-- != 0);
}

void AniArt_MZLava() {
	const uint8_t TILE_COUNT = 8;
	if (--level_anim[LAVA_ANIM].time < 0) {
		level_anim[LAVA_ANIM].time = 0x14 - 1;
		if (++level_anim[LAVA_ANIM].frame >= 3)
			level_anim[LAVA_ANIM].frame = 0;
		const uint8_t *src = Art_MZLava1 + (level_anim[LAVA_ANIM].frame * TILE_COUNT * TILE_SIZE);
		VDP_SeekVRAM(ArtTile_MZ_Animated_Magma * TILE_SIZE);
		LoadTiles(src, TILE_COUNT - 1);
	}
}

static const MagmaShiftFunc MagmaDistortionTable[] = {
	MagmaRow_Shift0,  MagmaRow_Shift1,  MagmaRow_Shift2,  MagmaRow_Shift3,
	MagmaRow_Shift4,  MagmaRow_Shift5,  MagmaRow_Shift6,  MagmaRow_Shift7,
	MagmaRow_Shift8,  MagmaRow_Shift9,  MagmaRow_Shift10, MagmaRow_Shift11,
	MagmaRow_Shift12, MagmaRow_Shift13, MagmaRow_Shift14, MagmaRow_Shift15
};

void AniArt_MZMagma() {
	if (--level_anim[MAGMA_ANIM].time < 0) {
		level_anim[MAGMA_ANIM].time = 2 - 1;
		uint32_t bank_offset = (uint32_t)level_anim[LAVA_ANIM].frame << 9;
		const uint8_t *magma_art = Art_MZLava2 + bank_offset;
		VDP_SeekVRAM(ArtTile_MZ_Animated_Lava * TILE_SIZE);
		uint8_t osc_val = (uint8_t)oscillatory.state[4][0];
		for (int chunk = 0; chunk < 4; chunk++) {
			uint8_t table_idx = (osc_val * 2) & 0x1E;
			MagmaShiftFunc ApplyShift = MagmaDistortionTable[(table_idx >> 1) & 15];
			ApplyShift(magma_art, 0x1F);
			osc_val += 4;
		}
	}
}

void AniArt_MZTorch() {
	const uint8_t TILE_COUNT = 6;
	if (--level_anim[TORCH_TIMER].time < 0) {
		level_anim[TORCH_TIMER].time = 8 - 1;
		uint8_t current_frame = level_anim[TORCH_ANIM].frame;
		level_anim[TORCH_ANIM].frame = (level_anim[TORCH_ANIM].frame + 1) & 3;
		const uint8_t *src = Art_MZTorch + (current_frame * TILE_COUNT * TILE_SIZE);
		VDP_SeekVRAM(ArtTile_MZ_Torch * TILE_SIZE);
		LoadTiles(src, TILE_COUNT - 1);
	}
}

void AnimateLevelGfx()
{
    // Don't run if game is paused
    if (pause)
        return;

    // Animate giant ring
    AniArt_GiantRing();

    // Run level animation
    switch (LEVEL_ZONE(level_id)) {
    case ZoneId_GHZ:
        AniArt_GHZWaterfall();
        AniArt_GHZFlowerLarge();
        AniArt_GHZFlowerSmall();
        break;
    case ZoneId_LZ:
        break;
    case ZoneId_MZ:
        AniArt_MZLava();
        AniArt_MZMagma();
        AniArt_MZTorch();
        break;
    case ZoneId_SLZ:
        break;
    case ZoneId_SYZ:
        break;
    case ZoneId_SBZ:
        break;
    case ZoneId_EndZ:
        break;
    }
}
