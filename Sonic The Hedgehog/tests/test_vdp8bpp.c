#include "test.h"

#include <string.h>

#include "Backend/VDP.h"

// The VDP's 8 bits a pixel tile row (not used by the planes and sprites yet): 64 byte tiles, a byte a pixel, the byte a colour RAM index over all 256 colours, 0 transparent.

// A tile whose pixel (x, y) is 16 * y + x + 1: every byte different, so a flip or a wrong row shows
static void LoadTile(size_t pattern) {
    uint8_t tile[64];
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            tile[y * 8 + x] = (uint8_t)(16 * y + x + 1);
    VDP_SeekVRAM(pattern << 6);
    VDP_WriteVRAM(tile, sizeof(tile));
    // colour index n is the true colour 0x00NN0000 + n, so a pixel says which index it came from
    static uint32_t colours[256];
    for (int i = 0; i < 256; i++)
        colours[i] = (uint32_t)((i << 16) | (i ^ 0xFF));
    VDP_SeekCRAM(0);
    VDP_WriteCRAM_RGB(colours, 256);
}

static uint32_t Px(int index) {
    return (uint32_t)(((index << 24) | ((index ^ 0xFF) << 8)) | 0xFF); // 0xRRGGBBAA of the colour above
}

static void Vdp8_RowComesFromTheRightLineOfTheTile(void) {
    LoadTile(3);
    uint32_t out[8];
    uint8_t mask[8];
    for (int y = 0; y < 8; y++) {
        memset(out, 0, sizeof(out));
        memset(mask, 0, sizeof(mask));
        VDP_DrawTileRow8(out, mask, 3, y, false, false, 0, 0);
        for (int x = 0; x < 8; x++)
            CHECK_EQ(out[x], Px(16 * y + x + 1));
    }
}

static void Vdp8_FlipsAreTheTilesOwn(void) {
    LoadTile(0);
    uint32_t out[8];
    uint8_t mask[8] = { 0 };
    VDP_DrawTileRow8(out, mask, 0, 2, true, false, 0, 0); // across
    for (int x = 0; x < 8; x++)
        CHECK_EQ(out[x], Px(16 * 2 + (7 - x) + 1));
    VDP_DrawTileRow8(out, mask, 0, 2, false, true, 0, 0); // down: row 2 shows row 5
    for (int x = 0; x < 8; x++)
        CHECK_EQ(out[x], Px(16 * 5 + x + 1));
    VDP_DrawTileRow8(out, mask, 0, 2, true, true, 0, 0);
    for (int x = 0; x < 8; x++)
        CHECK_EQ(out[x], Px(16 * 5 + (7 - x) + 1));
}

// Pixels of colour 0 draw nothing and set no mask; the rest use any of the 256 colours
static void Vdp8_ZeroIsTransparentAndTheRestReachAllTwoFiftySixColours(void) {
    uint8_t tile[64];
    memset(tile, 0, sizeof(tile));
    tile[0] = 255; // the last colour of the 16th line
    tile[1] = 0;
    tile[2] = 17;  // line 1, colour 1
    VDP_SeekVRAM(5 << 6);
    VDP_WriteVRAM(tile, sizeof(tile));
    static uint32_t colours[256];
    for (int i = 0; i < 256; i++)
        colours[i] = 0x00010203u + (uint32_t)i;
    VDP_SeekCRAM(0);
    VDP_WriteCRAM_RGB(colours, 256);

    uint32_t out[8];
    uint8_t mask[8] = { 0 };
    for (int i = 0; i < 8; i++)
        out[i] = 0xDEADBEEF;
    VDP_DrawTileRow8(out, mask, 5, 0, false, false, 0, 0x01);
    CHECK_EQ(out[0], ((0x00010203u + 255) << 8) | 0xFF);
    CHECK_EQ(out[1], 0xDEADBEEFu); // (transparent: untouched)
    CHECK_EQ(out[2], ((0x00010203u + 17) << 8) | 0xFF);
    CHECK_EQ(mask[0], 0x01);
    CHECK_EQ(mask[1], 0);
    CHECK_EQ(mask[2], 0x01);
}

// The priority masks, as the 4bpp rows do: a pixel over a mask bit of `and` is not drawn, and the mask gains `or` either way
static void Vdp8_PriorityMasksWork(void) {
    LoadTile(1);
    uint32_t out[8];
    uint8_t mask[8] = { 0x02, 0, 0x02, 0, 0, 0, 0, 0 };
    for (int i = 0; i < 8; i++)
        out[i] = 0xDEADBEEF;
    VDP_DrawTileRow8(out, mask, 1, 0, false, false, 0x02, 0x04);
    CHECK_EQ(out[0], 0xDEADBEEFu); // (hidden)
    CHECK_EQ(out[1], Px(2));
    CHECK_EQ(out[2], 0xDEADBEEFu);
    CHECK_EQ(mask[0], 0x06);
    CHECK_EQ(mask[1], 0x04);
}

// Tiles are 64 bytes apart: the next one is not the 4bpp tile after it
static void Vdp8_PatternsAreSixtyFourBytesApart(void) {
    LoadTile(2);
    uint8_t other[64];
    memset(other, 0x7F, sizeof(other));
    VDP_SeekVRAM(3 << 6);
    VDP_WriteVRAM(other, sizeof(other));
    uint32_t out[8];
    uint8_t mask[8] = { 0 };
    VDP_DrawTileRow8(out, mask, 2, 7, false, false, 0, 0);
    CHECK_EQ(out[7], Px(16 * 7 + 7 + 1)); // (the whole of tile 2, not spilling into 3)
    VDP_DrawTileRow8(out, mask, 3, 0, false, false, 0, 0);
    CHECK_EQ(out[0], Px(0x7F));
}

// The colours are read from the colour RAM as the row is drawn, so rewriting a range of it recolours the same tile with no new pixels: a palette cycle of an 8bpp tileset, over any of the 16 lines
static void Vdp8_ChangingTheColourRAMCyclesTheSameTile(void) {
    uint8_t tile[64];
    for (int i = 0; i < 64; i++)
        tile[i] = (uint8_t)(0xC1 + (i & 3)); // colours 0xC1 to 0xC4: line 12
    VDP_SeekVRAM(6 << 6);
    VDP_WriteVRAM(tile, sizeof(tile));

    static const uint32_t ramp[4] = { 0x00110000, 0x00220000, 0x00330000, 0x00440000 };
    uint32_t cycled[4];
    uint32_t out[8], first[8];
    uint8_t mask[8];
    for (int step = 0; step < 4; step++) {
        for (int i = 0; i < 4; i++)
            cycled[i] = ramp[(i + step) & 3]; // the ramp rotated one place a step
        VDP_SeekCRAM(0xC1);
        VDP_WriteCRAM_RGB(cycled, 4);
        memset(mask, 0, sizeof(mask));
        VDP_DrawTileRow8(out, mask, 6, 0, false, false, 0, 0);
        if (step == 0)
            memcpy(first, out, sizeof(out));
        for (int x = 0; x < 8; x++)
            CHECK_EQ(out[x], (ramp[((x & 3) + step) & 3] << 8) | 0xFF);
    }
    CHECK(first[0] != out[0] || first[1] != out[1]); // (it did change on the way)
    VDP_SeekCRAM(0);
    VDP_FillCRAM(0, COLOURS);
}

void RegisterVdp8bppTests(void) {
    RUN_TEST(Vdp8_RowComesFromTheRightLineOfTheTile);
    RUN_TEST(Vdp8_FlipsAreTheTilesOwn);
    RUN_TEST(Vdp8_ZeroIsTransparentAndTheRestReachAllTwoFiftySixColours);
    RUN_TEST(Vdp8_PriorityMasksWork);
    RUN_TEST(Vdp8_PatternsAreSixtyFourBytesApart);
    RUN_TEST(Vdp8_ChangingTheColourRAMCyclesTheSameTile);
}
