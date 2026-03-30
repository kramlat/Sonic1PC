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

// Block drawing functions
size_t CalcVRAMPos(int16_t sx, int16_t sy, int16_t x, int16_t y) {
    // Convert coordinates to plane coordinates
    uint16_t px = ((x + sx) >> 2) & ~3;
    uint16_t py = ((y + sy) >> 2) & ~3;
    return (POSITIVE_MOD(py, PLANE_HEIGHT << 1) * PLANE_WIDTH) + POSITIVE_MOD(px, PLANE_WIDTH << 1);
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

void DrawBlocks_TB(size_t offset, size_t pos, int16_t sx, int16_t sy, int16_t x, int16_t y, uint8_t* layout)
{
    DrawBlocks_TB_2(offset, pos, sx, sy, x, y, layout, (SCROLL_HEIGHT + 16 + 16) / 16);
}

void DrawBlocks_BG(size_t offset, int16_t sx, int16_t sy, int16_t y, uint8_t* layout, const uint8_t* array)
{
    static const dword_s* bg_pos[] = { &bg_scrpos_x, &bg_scrpos_x, &bg2_scrpos_x, &bg3_scrpos_y };
    uint8_t bg_pos_i = array[y >> 4];
    if (bg_pos_i != 0) {
        sx = bg_pos[bg_pos_i >> 1]->f.u;
        sy = (sy & ~0xF) % SCROLL_HEIGHT;
        DrawBlocks_LR(offset, CalcVRAMPos(sx, sy, 0, y), sx, sy, 0, y, layout);
    } else {
        DrawBlocks_LR_2(offset, CalcVRAMPos(sx, sy, 0, y), sx, sy, 0, y, layout, PLANE_WIDTH);
    }
}

void Draw_GHZ_Bg(int16_t sy, uint8_t* layout, size_t offset)
{
    int16_t y = 0;
    for (size_t i = 0; i < (SCROLL_HEIGHT + 16 + 16) / 16; i++) {
        static const uint8_t bg_array[] = { 0x00, 0x00, 0x00, 0x00, 0x06, 0x06, 0x06, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        DrawBlocks_BG(offset, bg_scrpos_y.f.u, sy, y, layout, bg_array);
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
    if (LEVEL_ZONE(level_id) == ZoneId_GHZ)
        Draw_GHZ_Bg(bg_scrpos_y.f.u, level_layout[0][1], VRAM_BG);
    else if (LEVEL_ZONE(level_id) == ZoneId_MZ) {
        ;
    } // Draw_MZ_Bg(bg_scrpos_y.f.u, level_layout[0][1], VRAM_BG);
    else if (level_id == LEVEL_ID(ZoneId_SBZ, 0)) {
        ;
    } // Draw_SBZ_Bg(bg_scrpos_y.f.u, level_layout[0][1], VRAM_BG);
    else if (LEVEL_ZONE(level_id) == ZoneId_EndZ)
        Draw_GHZ_Bg(bg_scrpos_y.f.u, level_layout[0][1], VRAM_BG);
    else
#endif
        DrawChunks(bg_scrpos_x.f.u, bg_scrpos_y.f.u, level_layout[0][1], VRAM_BG);
}

void DrawBGScrollBlock1(int16_t sx, int16_t sy, uint16_t* flag, uint8_t* layout, size_t offset)
{
    // TODO: REV00
    // Check if any flags have been set
    if (*flag == 0)
        return;

    // Handle flags
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
        DrawBlocks_LR_2(offset, CalcVRAMPos(0, sy, 0, -16), 0, sy, 0, -16, layout, PLANE_WIDTH / 2);
        *flag &= ~SCROLL_FLAG_UP2;
    }
    if (*flag & SCROLL_FLAG_DOWN2) {
        DrawBlocks_LR_2(offset, CalcVRAMPos(0, sy, 0, SCROLL_HEIGHT), 0, sy, 0, SCROLL_HEIGHT, layout, PLANE_WIDTH / 2);
        *flag &= ~SCROLL_FLAG_DOWN2;
    }
}

void DrawBGScrollBlock2(int16_t sx, int16_t sy, uint16_t* flag, uint8_t* layout, size_t offset)
{
    // TODO: REV00
    // Check if any flags have been set
    if (*flag == 0)
        return;

    // Run completely different code if in Scrap Brain Zone (what)
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
        // TODO
    }
}

void DrawBGScrollBlock3(int16_t sx, int16_t sy, uint16_t* flag, uint8_t* layout, size_t offset)
{
    // TODO: REV00
    // Check if any flags have been set
    if (*flag == 0)
        return;

    // Run completely different code if in Marble Zone (what)
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
        // TODO
    }
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
