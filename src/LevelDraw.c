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

size_t CalcVRAMPos_2(int16_t sx, int16_t x, int16_t y) {
    uint16_t py = (uint16_t)y & 0x00F0;
    uint16_t px = (uint16_t)(x + sx) & 0x01F0;
    return (size_t)((py << 4) + (px >> 2));
}

size_t CalcVRAMPos(int16_t sx, int16_t sy, int16_t x, int16_t y) {
    // Calculate the absolute World Y position.
    int16_t py = y + sy;
    return CalcVRAMPos_2(sx, x, py);
}

/**
 * Historical/Dead Code Version (CalcVRAMPos_Unknown)
 *
 * This version exists in the original retail code but is never called.
 *
 * ARCHAEOLOGY:
 * In the original assembly, this returned a "VDP Command" with a prefix
 * of 0x0002. In Mega Drive terms, that refers to memory address $8000.
 *
 * This tells us that early in Sonic 1's development, the "Plane A"
 * nametable was likely located at $8000 in VRAM. It was moved to $C000
 * later in development, and this function was left behind as "dead code."
 */
size_t CalcVRAMPos_Unknown(int16_t sx, int16_t sy, int16_t x, int16_t y) {
    // The geometry logic is identical to the standard version.
    return CalcVRAMPos(sx, sy, x, y);
}

/**
 * Primary entry point: Calculates block data by first applying the camera's X offset.
 */
void GetBlockData(const uint8_t** meta, const uint8_t** block, int16_t sx, int16_t sy, int16_t x, int16_t y, uint8_t* layout) {
    // Apply camera X-offset before processing
    x += sx;
    GetBlockData_2(meta, block, sx, sy, x, y, layout);
}

/**
 * Secondary entry point: Calculates the memory addresses for block metadata and
 * tile data based on world coordinates and the level layout.
 */
void GetBlockData_2(const uint8_t** meta, const uint8_t** block, int16_t sx, int16_t sy, int16_t x, int16_t y, uint8_t* layout) {
    // Apply camera Y-offset
    y += sy;

    // 1. Determine which 512x512 chunk this coordinate falls into within the layout
    // The layout is a grid of bytes where each byte is a Chunk ID.
    uint16_t layout_row = ((uint16_t)y >> 1) & 0x380;
    uint16_t layout_col = ((uint16_t)x >> 8) & 0x07F;
    uint8_t chunk_id = layout[layout_row + layout_col];

    // If chunk ID is 0, it is considered empty space.
    if (chunk_id == 0) {
        *meta = level_map256;
        *block = level_map16;
        return;
    }

    // 2. Calculate the offset into the Chunk Table.
    // Each chunk is 512 bytes (16x16 blocks, 2 bytes per block entry).
    // (chunk_id - 1) * 512
    uint32_t chunk_offset = (uint32_t)((chunk_id - 1) & 0x7F) << 9;

    // 3. Find the specific 16x16 block within that chunk.
    // Coordinates are masked and scaled to point to a 2-byte word entry.
    uint16_t inner_y_offset = ((uint16_t)y << 1) & 0x1E0;
    uint16_t inner_x_offset = ((uint16_t)x >> 3) & 0x01E;

    // Set the metadata pointer (a0 in assembly)
    const uint8_t* metadata_ptr = level_map256 + chunk_offset + inner_y_offset + inner_x_offset;
    *meta = metadata_ptr;

    // 4. Retrieve the Block ID from the metadata and calculate the tile data address.
    // Block IDs are 10-bit values stored as Big-Endian words.
    uint16_t block_id = (metadata_ptr[0] << 8) | metadata_ptr[1];
    block_id &= 0x3FF;

    // Each block in the tilemap table is 8 bytes long.
    *block = level_map16 + (block_id << 3);
}

#define WRITE_TILE(off, xor)                                    \
    {                                                           \
        VDP_SeekVRAM(offset + (off));                           \
        uint16_t v = ((block[0] << 8) | (block[1] << 0)) ^ xor; \
        block += 2;                                             \
        VDP_WriteVRAM((const uint8_t*)&v, 2);                   \
    }

/**
 * Helper to write two tiles at once, mirroring the 68k 'move.l (a1)+, (a6)' behavior.
 * This fetches 4 bytes from the block pointer, modifies them, and writes to VRAM.
 */
static inline void WriteTwoTiles(const uint8_t* tiles, size_t vram_addr, uint16_t xor_val, bool swap_order) {
    VDP_SeekVRAM(vram_addr);

    // Convert bytes to words (Big Endian as per 68k)
    uint16_t t1 = (tiles[0] << 8) | tiles[1];
    uint16_t t2 = (tiles[2] << 8) | tiles[3];

    t1 ^= xor_val;
    t2 ^= xor_val;

    if (swap_order) {
        // swap d4 - puts the second tile first
        uint16_t v1 = t2;
        uint16_t v2 = t1;
        VDP_WriteVRAM((const uint8_t*)&v1, 2);
        VDP_WriteVRAM((const uint8_t*)&v2, 2);
    } else {
        VDP_WriteVRAM((const uint8_t*)&t1, 2);
        VDP_WriteVRAM((const uint8_t*)&t2, 2);
    }
}

/**
 * DrawFlipXY: Handles both X and Y flipping.
 * Mirrors ASM DrawFlipXY: Swaps row order and swaps tile order within rows.
 */
void DrawFlipXY(const uint8_t* block, size_t offset) {
    // ASM Logic: d5 = Row1, d4 = Row2. Writes d4 (Row2) then d5 (Row1).
    // Both are XOR'd with 0x1800 and swapped.

    // Write Row 2 data into Row 1 position
    WriteTwoTiles(block + 4, offset, 0x1800, true);

    // Write Row 1 data into Row 2 position
    WriteTwoTiles(block + 0, offset + (PLANE_WIDTH << 1), 0x1800, true);
}

/**
 * DrawFlipY: Handles vertical flipping.
 * Mirrors ASM DrawFlipY: Swaps row order.
 */
void DrawFlipY(const uint8_t* block, size_t offset) {
    // ASM Logic: d5 = Row1, d4 = Row2. Writes d4 (Row2) then d5 (Row1).
    // Both are XOR'd with 0x1000.

    // Write Row 2 data into Row 1 position
    WriteTwoTiles(block + 4, offset, 0x1000, false);

    // Write Row 1 data into Row 2 position
    WriteTwoTiles(block + 0, offset + (PLANE_WIDTH << 1), 0x1000, false);
}

/**
 * DrawFlipX: Handles horizontal flipping.
 * Mirrors ASM DrawFlipX: Swaps tile order within rows.
 */
void DrawFlipX(const uint8_t* block, size_t offset) {
    // ASM Logic: Row1 XOR 0x0800 and swap. Row2 XOR 0x0800 and swap.

    // Write Row 1
    WriteTwoTiles(block + 0, offset, 0x0800, true);

    // Write Row 2
    WriteTwoTiles(block + 4, offset + (PLANE_WIDTH << 1), 0x0800, true);
}

/**
 * DrawBlock: Main entry point for drawing a 2x2 block.
 *
 * @param meta   Pointer to metadata (flag byte)
 * @param block  Pointer to the 4 tiles (8 bytes)
 * @param offset VRAM destination address
 */
void DrawBlock(const uint8_t* meta, const uint8_t* block, size_t offset) {
    uint8_t flags = meta[0];

    if (flags & 0x10) {        // Check Y-flip bit (btst #4)
        if (flags & 0x08) {    // Check X-flip bit (btst #3)
            DrawFlipXY(block, offset);
        } else {
            DrawFlipY(block, offset);
        }
    } else if (flags & 0x08) { // Check X-flip bit
        DrawFlipX(block, offset);
    } else {
        // Normal Draw (No flip)
        WriteTwoTiles(block + 0, offset, 0x0000, false);
        WriteTwoTiles(block + 4, offset + (PLANE_WIDTH << 1), 0x0000, false);
    }
}

/**
 * Core loop for drawing blocks from left to right.
 * Handles VRAM row wrapping and coordinate incrementing.
 */
void DrawBlocks_LR_2(size_t offset, size_t pos, int16_t sx, int16_t sy, int16_t x, int16_t y, uint8_t* layout, size_t width) {
    const uint8_t* meta;
    const uint8_t* block;
    while (width-- > 0) {
        /* Retrieve block pointers based on current world and scroll coordinates */
        GetBlockData(&meta, &block, sx, sy, x, y, layout);
        /* Render the 16x16 block to the VRAM destination */
        DrawBlock(meta, block, offset + pos);

        /*
         * Advance the VRAM position by 2 tiles (4 bytes).
         * The bitwise logic ensures the position wraps within the current 128-byte VRAM row.
         */
        size_t tx = pos % (PLANE_WIDTH << 1);
        size_t ty = pos / (PLANE_WIDTH << 1);
        pos = (ty * (PLANE_WIDTH << 1)) + ((tx + 4) % (PLANE_WIDTH << 1));
        /* Move to the next 16-pixel block on the X axis */
        x += 16;
    }
}

#ifdef SCP_REV01
/**
 * Variant of the horizontal draw loop using the alternative data retrieval method.
 */
void DrawBlocks_LR_3(size_t offset, size_t pos, int16_t sx, int16_t sy, int16_t x, int16_t y, uint8_t* layout, size_t width) {
    const uint8_t* meta;
    const uint8_t* block;

    while (width-- > 0) {
        GetBlockData_2(&meta, &block, sx, sy, x, y, layout);

        DrawBlock(meta, block, offset + pos);

        /* Update VRAM position with horizontal row wrapping */
        size_t row_base = pos & ~(PLANE_ROW_BYTES - 1);
        size_t column_offset = (pos + 4) & (PLANE_ROW_BYTES - 1);
        pos = row_base | column_offset;

        x += 16;
    }
}
#endif

/**
 * Renders a horizontal row of blocks across the screen width.
 * Used when the camera moves vertically to refresh the horizontal span.
 */
void DrawBlocks_LR(size_t offset, size_t pos, int16_t sx, int16_t sy, int16_t x, int16_t y, uint8_t* layout) {
    /* Calculate number of 16x16 blocks to cover 320px screen plus 16px margins on both sides */
    const size_t blocks_to_draw = (SCROLL_WIDTH + 16 + 16) / 16;
    DrawBlocks_LR_2(offset, pos, sx, sy, x, y, layout, (SCROLL_WIDTH + 16 + 16) / 16);
}

void DrawBlocks_TB_2(size_t offset, size_t pos, int16_t sx, int16_t sy, int16_t x, int16_t y, uint8_t* layout, size_t height) {
    const uint8_t* meta;
    const uint8_t* block;
    while (height-- > 0) {
        GetBlockData(&meta, &block, sx, sy, x, y, layout);
        DrawBlock(meta, block, offset + pos);
        size_t tx = pos % (PLANE_WIDTH << 1);
        size_t ty = pos / (PLANE_WIDTH << 1);
        pos = (((ty + 2) % PLANE_HEIGHT) * (PLANE_WIDTH << 1)) + tx;
        y += 16;
    }
}

void DrawBlocks_TB(size_t offset, size_t pos, int16_t sx, int16_t sy, int16_t x, int16_t y, uint8_t* layout) {
    DrawBlocks_TB_2(offset, pos, sx, sy, x, y, layout, (SCROLL_HEIGHT + 16 + 16) / 16);
}

const dword_s* bg_pos_table[] = {
    &bg_scrpos_x,  // Index 0
    &bg_scrpos_x,  // Index 2
    &bg2_scrpos_x, // Index 4
    &bg3_scrpos_y  // Index 6 (Kept .y as per existing prototyping, though ASM used .x)
};

void DrawBlocks_BG(size_t offset, int16_t sx, int16_t sy, int16_t y, uint8_t* layout, const uint8_t* scroll_mapping) {
    uint8_t scroll_reg_index = scroll_mapping[((sy + y) & 0x7F0) >> 4];
    if (scroll_reg_index != 0) {
        sx = bg_pos_table[scroll_reg_index >> 1]->f.u;
        int16_t aligned_sy = (sy & ~0xF) % SCROLL_HEIGHT;
        DrawBlocks_LR(offset, CalcVRAMPos(sx, aligned_sy, 0, y), sx, aligned_sy, 0, y, layout);
    } else {
        DrawBlocks_LR_2(offset, CalcVRAMPos(sx, sy, 0, y), sx, sy, 0, y, layout, PLANE_WIDTH);
    }
}

void Draw_GHZ_Bg(int16_t sy, uint8_t* layout, size_t offset) {
    static const uint8_t bg_array[] = { 0x00, 0x00, 0x00, 0x00, 0x06, 0x06, 0x06, 0x04,0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    int16_t y = 0;
    for (size_t i = 0; i < (SCROLL_HEIGHT + 16 + 16) / 16; i++) {
        DrawBlocks_BG(offset, bg_scrpos_y.f.u, sy, y, layout, bg_array);
        y += 16;
    }
}

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


void Draw_MZ_Bg(int16_t sy, uint8_t* layout, size_t offset) {
    int16_t y = -16;
    for (size_t i = 0; i < (224 + 16 + 16) / 16; i++) {
        DrawBlocks_BG(offset, bg_scrpos_y.f.u, sy - 0x200, y, layout, MZ_ScrollArray);
        y += 16;
    }
}

const uint8_t SBZ_ScrollArray[32] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06
    };

void Draw_SBZ_Bg(int16_t sy, uint8_t* layout, size_t offset) {
    int16_t y = -16;
    for (size_t i = 0; i < (224 + 16 + 16) / 16; i++) {
        DrawBlocks_BG(offset, bg_scrpos_y.f.u, sy, y, layout, SBZ_ScrollArray);
        y += 16;
    }
}

// Level drawing functions
void DrawChunks(int16_t sx, int16_t sy, uint8_t* layout, size_t offset)
{
    int16_t y = -16;
    for (size_t i = 0; i < (SCROLL_HEIGHT + 16 + 16) / 16; i++) {
        DrawBlocks_LR_2(offset, CalcVRAMPos(sx, sy, 0, y), sx, sy, 0, y, layout, PLANE_WIDTH / 2);
        y += 16;
    }
}

void LoadTilesFromStart()
{
    DrawChunks(scrpos_x.f.u, scrpos_y.f.u, level_layout[0][0], VRAM_FG);
#ifndef SCP_REV00
    if (LEVEL_ZONE(level_id) == ZoneId_GHZ || LEVEL_ZONE(level_id) == ZoneId_EndZ)
        Draw_GHZ_Bg(bg_scrpos_y.f.u, level_layout[0][1], VRAM_BG);
    else if (LEVEL_ZONE(level_id) == ZoneId_MZ)
        Draw_MZ_Bg(bg_scrpos_y.f.u, level_layout[0][1], VRAM_BG);
    else if (level_id == LEVEL_ID(ZoneId_SBZ, 0))
        Draw_SBZ_Bg(bg_scrpos_y.f.u, level_layout[0][1], VRAM_BG);
    else
#endif
        DrawChunks(bg_scrpos_x.f.u, bg_scrpos_y.f.u, level_layout[0][1], VRAM_BG);
}

void DrawBGScrollBlock1(int16_t sx, int16_t sy, uint16_t* flag, uint8_t* layout, size_t offset) {
    // tst.b (a2) - Test the lower byte for active flags
    if ((*flag & 0xFF) == 0)
        return;

#ifdef SCP_REV00
    if (*flag & SCROLL_FLAG_UP) {
        *flag &= ~SCROLL_FLAG_UP;
        size_t pos = CalcVRAMPos(sx, sy, -16, -16);
        DrawBlocks_LR_2(offset, pos, sx, sy, -16, -16, layout, (512 / 16));
    }
    if (*flag & SCROLL_FLAG_DOWN) {
        *flag &= ~SCROLL_FLAG_DOWN;
        size_t pos = CalcVRAMPos(sx, sy, -16, 224);
        DrawBlocks_LR_2(offset, pos, sx, sy, -16, 224, layout, (512 / 16));
    }
    if (*flag & SCROLL_FLAG_LEFT) {
        *flag &= ~SCROLL_FLAG_LEFT;
        int16_t size1 = scroll_block1_size - (sy & -16);
        if (size1 >= 0) {
            size1 >>= 4; // Convert to blocks
            if (size1 > ((224 + 16 + 16) / 16) - 1)
                size1 = ((224 + 16 + 16) / 16) - 1;
            size_t pos = CalcVRAMPos(sx, sy, -16, -16);
            DrawBlocks_TB_2(offset, pos, sx, sy, -16, -16, layout, size);
        }
    }
    if (*flag & SCROLL_FLAG_RIGHT) {
        *flag &= ~SCROLL_FLAG_RIGHT;
        int16_t size = v_scroll_block_1_size - (sy & -16);
        if (size >= 0) {
            size >>= 4;
            if (size > ((224 + 16 + 16) / 16) - 1)
                size = ((224 + 16 + 16) / 16) - 1;
            size_t pos = CalcVRAMPos(sx, sy, 320, -16);
            DrawBlocks_TB_2(offset, pos, sx, sy, 320, -16, layout, size);
        }
    }

#else
    if (*flag & SCROLL_FLAG_UP) {
        *flag &= ~SCROLL_FLAG_UP;
        size_t pos = CalcVRAMPos(sx, sy, -16, -16);
        DrawBlocks_LR(offset, pos, sx, sy, -16, -16, layout);
    }
    if (*flag & SCROLL_FLAG_DOWN) {
        *flag &= ~SCROLL_FLAG_DOWN;
        size_t pos = CalcVRAMPos(sx, sy, -16, 224);
        DrawBlocks_LR(offset, pos, sx, sy, -16, 224, layout);
    }
    if (*flag & SCROLL_FLAG_LEFT) {
        *flag &= ~SCROLL_FLAG_LEFT;
        size_t pos = CalcVRAMPos(sx, sy, -16, -16);
        DrawBlocks_TB(offset, pos, sx, sy, -16, -16, layout);
    }
    if (*flag & SCROLL_FLAG_RIGHT) {
        *flag &= ~SCROLL_FLAG_RIGHT;
        size_t pos = CalcVRAMPos(sx, sy, 320, -16);
        DrawBlocks_TB(offset, pos, sx, sy, 320, -16, layout);
    }
    if (*flag & SCROLL_FLAG_UP2) {
        *flag &= ~SCROLL_FLAG_UP2;
        size_t pos = CalcVRAMPos_2(sx, 0, -16);
        DrawBlocks_LR_3(offset, pos, sx, sy, 0, -16, layout, (512 / 16));
    }
    if (*flag & SCROLL_FLAG_DOWN2) {
        *flag &= ~SCROLL_FLAG_DOWN2;
        size_t pos = CalcVRAMPos_2(sx, 0, 224);
        DrawBlocks_LR_3(offset, pos, sx, sy, 0, 224, layout, (512 / 16));
    }
#endif
}

void DrawBGScrollBlock2(int16_t sx, int16_t sy, uint16_t* scroll_flags, uint8_t* layout, size_t offset) {
    if ((*scroll_flags & 0xFF) == 0)
        return;

#if SCP_REV01
    if (LEVEL_ZONE(level_id) == ZoneId_SBZ) {
        int16_t vertical_offset = -16; // Margin for drawing new tiles
        if (*scroll_flags & 0x01) {
            *scroll_flags &= ~0x01;
            DrawBlocks_BG(offset, sx, bg_scrpos_y.f.u, vertical_offset, layout, SBZ_ScrollArray);
        } else if (*scroll_flags & 0x02) {
            *scroll_flags &= ~0x02;
            vertical_offset = 224; // Draw slice at the bottom of the screen
            DrawBlocks_BG(offset, sx, bg_scrpos_y.f.u, vertical_offset, layout, SBZ_ScrollArray);
        }
        uint8_t sync_bits = (*scroll_flags & 0xA8);
        if (sync_bits != 0) {
            // Shift flags to the next state and trigger a right-side update
            *scroll_flags = (uint16_t)(sync_bits >> 1);
            int16_t sync_y_off = -16;
            DrawBlocks_BG(offset, sx, bg_scrpos_y.f.u, sync_y_off, layout, SBZ_ScrollArray);
        }
        return;
    }
#endif

#if SCP_REV00
    if (*scroll_flags & 0x04) {
        *scroll_flags &= ~0x04;
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
    if (*scroll_flags & 0x08) {
        *scroll_flags &= ~0x08;
        int16_t current_sy = sy & -16;
        int16_t remaining_coverage = scroll_block1_size - current_sy;
        int16_t row_count = (remaining_coverage >> 4) - 14;
        if (row_count < 0) {
            size_t vram_pos = CalcVRAMPos(sx, sy, 320, remaining_coverage);
            DrawBlocks_TB_2(offset, vram_pos, sx, sy, 320, remaining_coverage, layout, -row_count);
        }
    }
#elif SCP_REV01
    if (*scroll_flags & 0x01) {
        *scroll_flags &= ~0x01;
        size_t vram_pos = CalcVRAMPos(sx, sy, -16, 112); // 224/2
        DrawBlocks_TB_2(offset, vram_pos, sx, sy, -16, 112, layout, 3);
    }
    if (*scroll_flags & 0x02) {
        *scroll_flags &= ~0x02;
        size_t vram_pos = CalcVRAMPos(sx, sy, 320, 112);
        DrawBlocks_TB_2(offset, vram_pos, sx, sy, 320, 112, layout, 3);
    }
#endif
}

void DrawBGScrollBlock3(int16_t sx, int16_t sy, uint16_t* flag, uint8_t* layout, size_t offset) {
    if ((*flag & 0xFF) == 0) return;

#ifdef SCP_REV00
    if (*flag & SCROLL_FLAG_LEFT2) {
        *flag &= ~SCROLL_FLAG_LEFT2;
        int16_t dy = (224 - 16) - (sy & -16);
        DrawBlocks_TB_2(offset, CalcVRAMPos_Unknown(sx, sy, -16, dy), sx, sy, -16, dy, layout, 3);
    }
    if (*flag & SCROLL_FLAG_RIGHT2) {
        *flag &= ~SCROLL_FLAG_RIGHT2;
        int16_t dy = (224 - 16) - (sy & -16);
        DrawBlocks_TB_2(offset, CalcVRAMPos_Unknown(sx, sy, SCROLL_WIDTH, dy), sx, sy, SCROLL_WIDTH, dy, layout, 3);
    }
#else
    if (LEVEL_ZONE(level_id) == ZoneId_MZ) {
        int16_t y_rel = -16;
        if (!(*flag & SCROLL_FLAG_LEFT)) {
            if (*flag & SCROLL_FLAG_RIGHT) {
                *flag &= ~SCROLL_FLAG_RIGHT;
                y_rel = 224;
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
    } else {
        if (*flag & SCROLL_FLAG_LEFT) {
            *flag &= ~SCROLL_FLAG_LEFT;
            DrawBlocks_TB_2(offset, CalcVRAMPos(sx, sy, -16, 64), sx, sy, -16, 64, layout, 3);
        }
        if (*flag & SCROLL_FLAG_RIGHT) {
            *flag &= ~SCROLL_FLAG_RIGHT;
            DrawBlocks_TB_2(offset, CalcVRAMPos(sx, sy, SCROLL_WIDTH, 64), sx, sy, SCROLL_WIDTH, 64, layout, 3);
        }
    }
#endif
}

void LoadTilesAsYouMove()
{
    // Scroll background
    DrawBGScrollBlock1(bg_scrpos_x.f.u, bg_scrpos_y.f.u, &bg1_scroll_flags, level_layout[0][1], VRAM_BG);
    DrawBGScrollBlock2(bg2_scrpos_x.f.u, bg2_scrpos_y.f.u, &bg2_scroll_flags, level_layout[0][1], VRAM_BG);
    DrawBGScrollBlock3(bg3_scrpos_x.f.u, bg3_scrpos_y.f.u, &bg3_scroll_flags, level_layout[0][1], VRAM_BG);

    // Scroll foreground
    int16_t sx = scrpos_x_dup.f.u;
    int16_t sy = scrpos_y_dup.f.u;
    uint8_t* layout = level_layout[0][0];

    if (fg_scroll_flags == 0)
        return;

    if (fg_scroll_flags & SCROLL_FLAG_UP) {
        DrawBlocks_LR(VRAM_FG, CalcVRAMPos(sx, sy, -16, -16), sx, sy, -16, -16, layout);
        fg_scroll_flags &= ~SCROLL_FLAG_UP;
    }
    if (fg_scroll_flags & SCROLL_FLAG_DOWN) {
        DrawBlocks_LR(VRAM_FG, CalcVRAMPos(sx, sy, -16, SCROLL_HEIGHT), sx, sy, -16, SCROLL_HEIGHT, layout);
        fg_scroll_flags &= ~SCROLL_FLAG_DOWN;
    }
    if (fg_scroll_flags & SCROLL_FLAG_LEFT) {
        DrawBlocks_TB(VRAM_FG, CalcVRAMPos(sx, sy, -16, -16), sx, sy, -16, -16, layout);
        fg_scroll_flags &= ~SCROLL_FLAG_LEFT;
    }
    if (fg_scroll_flags & SCROLL_FLAG_RIGHT) {
        DrawBlocks_TB(VRAM_FG, CalcVRAMPos(sx, sy, SCROLL_WIDTH, -16), sx, sy, SCROLL_WIDTH, -16, layout);
        fg_scroll_flags &= ~SCROLL_FLAG_RIGHT;
    }
}

void LoadTilesAsYouMove_BGOnly()
{
    DrawBGScrollBlock1(bg_scrpos_x.f.u, bg_scrpos_y.f.u, &bg1_scroll_flags, level_layout[0][1], VRAM_BG);
    DrawBGScrollBlock2(bg2_scrpos_x.f.u, bg2_scrpos_y.f.u, &bg2_scroll_flags, level_layout[0][1], VRAM_BG);
    // No scroll block 3, even in REV01... odd
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
