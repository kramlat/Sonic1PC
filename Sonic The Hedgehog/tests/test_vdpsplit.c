#include "test.h"

#include <string.h>

#include "Video.h"
#include "Backend/VDP.h"

// The VDP's split screen (engine/Backend/VDP.c): one view alone, two stacked in the double-height mode, two side by side.

#define RED   0x000E
#define GREEN 0x00E0
#define BLUE  0x0E00
#define PIXEL_RED   0xFF0000FFu
#define PIXEL_GREEN 0x00FF00FFu
#define PIXEL_BLUE  0x0000FFFFu
#define PIXEL_BACKDROP 0x000000FFu

#define PLANE_A  0x10000
#define PLANE_B  0x12000
#define PLANE_A2 0x14000
#define HSCROLL  0xFC00
#define HSCROLL2 0xFE00

static void SetPattern(unsigned pattern, uint8_t colour) {
    uint8_t bytes[32];
    memset(bytes, (colour << 4) | colour, sizeof(bytes));
    VDP_SeekVRAM(pattern * 32);
    VDP_WriteVRAM(bytes, sizeof(bytes));
}

static void SetName(size_t plane, unsigned col, unsigned row, uint16_t entry) {
    VDP_SeekVRAM(plane + (row * 64 + col) * 2);
    VDP_WriteVRAM((const uint8_t *)&entry, 2);
}

static void Prepare(void) {
    VDP_SetSplitScreen(VDP_SPLIT_NONE, NULL);
    VDP_SeekVRAM(0);
    VDP_FillVRAM(0, VRAM_SIZE);
    uint16_t cram[64];
    memset(cram, 0, sizeof(cram));
    cram[1] = RED; cram[2] = GREEN; cram[3] = BLUE;
    VDP_SeekCRAM(0);
    VDP_WriteCRAM(cram, 64);
    VDP_SetPlaneALocation(PLANE_A);
    VDP_SetPlaneBLocation(PLANE_B);
    VDP_SetHScrollLocation(HSCROLL);
    VDP_SetPlaneSize(64, 32);
    VDP_SetBackgroundColour(0);
    VDP_SetVScroll(0, 0);
    VDP_SetSpriteBuffer(NULL);
    VDP_SetSpriteLocation(0xF800);
    VDP_SetHIntEnable(false);
}

static uint32_t Px(const uint32_t *frame, int pitch, int x, int y) {
    return frame[y * pitch + x];
}

static void VDPSplit_OneViewIsAsItWas(void) {
    Prepare();
    SetPattern(1, 1);
    SetName(PLANE_A, 0, 0, 1);
    VDP_DrawFrame();
    int pitch, rows;
    const uint32_t *f = VDP_GetFrame(&pitch, &rows);
    CHECK_EQ(rows, SCREEN_HEIGHT);
    CHECK_EQ(Px(f, pitch, 0, 0), PIXEL_RED);
    CHECK_EQ(Px(f, pitch, 7, 7), PIXEL_RED);
    CHECK_EQ(Px(f, pitch, 8, 0), PIXEL_BACKDROP);
    CHECK_EQ(Px(f, pitch, 0, 8), PIXEL_BACKDROP);
}

static void VDPSplit_StackedIsTwiceTheRowsWithTallCells(void) {
    Prepare();
    // Double-height cells: name 1 is the pair of patterns 2 (rows 0-7) and 3 (rows 8-15)
    SetPattern(2, 1);
    SetPattern(3, 2);
    SetPattern(4, 3); // name 2: patterns 4 and 5
    SetPattern(5, 3);
    SetName(PLANE_A, 0, 0, 1);
    SetName(PLANE_A2, 0, 0, 2);

    VDPView second;
    memset(&second, 0, sizeof(second));
    second.plane_a_location = PLANE_A2;
    second.plane_b_location = PLANE_B;
    second.hscroll_location = HSCROLL2;
    VDP_SetSplitScreen(VDP_SPLIT_STACKED, &second);
    CHECK_EQ(VDP_OutputRows(), 2 * SCREEN_HEIGHT);

    VDP_DrawFrame();
    int pitch, rows;
    const uint32_t *f = VDP_GetFrame(&pitch, &rows);
    CHECK_EQ(rows, 2 * SCREEN_HEIGHT);
    // The first view: a cell is 16 lines, its two patterns one above the other
    CHECK_EQ(Px(f, pitch, 0, 0), PIXEL_RED);
    CHECK_EQ(Px(f, pitch, 0, 7), PIXEL_RED);
    CHECK_EQ(Px(f, pitch, 0, 8), PIXEL_GREEN);
    CHECK_EQ(Px(f, pitch, 0, 15), PIXEL_GREEN);
    CHECK_EQ(Px(f, pitch, 0, 16), PIXEL_BACKDROP);
    // The second view starts a screen's height down, with its own plane
    CHECK_EQ(Px(f, pitch, 0, SCREEN_HEIGHT), PIXEL_BLUE);
    CHECK_EQ(Px(f, pitch, 0, SCREEN_HEIGHT + 15), PIXEL_BLUE);
    CHECK_EQ(Px(f, pitch, 0, SCREEN_HEIGHT + 16), PIXEL_BACKDROP);
    VDP_SetSplitScreen(VDP_SPLIT_NONE, NULL);
}

static void VDPSplit_StackedSpritesAreInDoubleHeightCoordinates(void) {
    Prepare();
    SetPattern(8, 3); // a sprite cell with tile number 4 is the patterns 8 and 9
    SetPattern(9, 3);
    SetPattern(12, 2); // tile 6: patterns 12 and 13
    SetPattern(13, 2);
    static uint16_t table1[2][4], table2[2][4];
    memset(table1, 0, sizeof(table1));
    memset(table2, 0, sizeof(table2));
    // One cell (width 0, height 0) at x 10, line 5 of the first view (Y 0x100 + 5); the second view's sprite at line 3 of its own (0x100 + height + 3)
    table1[0][0] = 0x100 + 5;  table1[0][1] = 0x0000; table1[0][2] = 4; table1[0][3] = 128 + 10;
    table2[0][0] = 0x100 + SCREEN_HEIGHT + 3; table2[0][1] = 0x0000; table2[0][2] = 6; table2[0][3] = 128 + 20;
    VDP_SetSpriteBuffer(&table1[0][0]);
    VDPView second;
    memset(&second, 0, sizeof(second));
    second.plane_a_location = PLANE_A2;
    second.plane_b_location = PLANE_B;
    second.hscroll_location = HSCROLL2;
    second.sprite_buffer = &table2[0][0];
    VDP_SetSplitScreen(VDP_SPLIT_STACKED, &second);
    VDP_DrawFrame();
    int pitch, rows;
    const uint32_t *f = VDP_GetFrame(&pitch, &rows);
    CHECK_EQ(Px(f, pitch, 10, 4), PIXEL_BACKDROP);
    CHECK_EQ(Px(f, pitch, 10, 5), PIXEL_BLUE);        // the cell is 16 lines tall
    CHECK_EQ(Px(f, pitch, 17, 20), PIXEL_BLUE);
    CHECK_EQ(Px(f, pitch, 10, 21), PIXEL_BACKDROP);
    CHECK_EQ(Px(f, pitch, 18, 10), PIXEL_BACKDROP);   // and 8 wide
    CHECK_EQ(Px(f, pitch, 20, SCREEN_HEIGHT + 2), PIXEL_BACKDROP);
    CHECK_EQ(Px(f, pitch, 20, SCREEN_HEIGHT + 3), PIXEL_GREEN); // the second view's sprite, in its own half
    CHECK_EQ(Px(f, pitch, 20, 3), PIXEL_BACKDROP);               // not in the first
    VDP_SetSplitScreen(VDP_SPLIT_NONE, NULL);
    VDP_SetSpriteBuffer(NULL);
}

static void VDPSplit_SideBySideHasEachViewItsOwnHalf(void) {
    Prepare();
    SetPattern(1, 1);
    SetPattern(2, 1);
    SetName(PLANE_A, 0, 0, 1);
    SetName(PLANE_A2, 0, 0, 2);
    uint16_t cram2[64];
    memset(cram2, 0, sizeof(cram2));
    cram2[1] = BLUE; // the second view shows colour 1 as blue: its own palette
    VDPView second;
    memset(&second, 0, sizeof(second));
    second.plane_a_location = PLANE_A2;
    second.plane_b_location = PLANE_B;
    second.hscroll_location = HSCROLL2;
    second.palette = cram2;
    VDP_SetSplitScreen(VDP_SPLIT_SIDE, &second);
    CHECK_EQ(VDP_OutputRows(), SCREEN_HEIGHT);
    VDP_DrawFrame();
    int pitch, rows;
    const uint32_t *f = VDP_GetFrame(&pitch, &rows);
    CHECK_EQ(Px(f, pitch, 0, 0), PIXEL_RED);                          // the left half: the first view
    CHECK_EQ(Px(f, pitch, SCREEN_WIDTH / 2 - 1, 0), PIXEL_BACKDROP);
    CHECK_EQ(Px(f, pitch, SCREEN_WIDTH / 2, 0), PIXEL_BLUE);          // the right half: the second view's plane and palette, from its own left edge
    CHECK_EQ(Px(f, pitch, SCREEN_WIDTH / 2 + 7, 7), PIXEL_BLUE);
    CHECK_EQ(Px(f, pitch, SCREEN_WIDTH / 2 + 8, 0), PIXEL_BACKDROP);
    VDP_SetSplitScreen(VDP_SPLIT_NONE, NULL);
}

static void VDPSplit_SecondViewHasItsOwnHScrollTable(void) {
    Prepare();
    SetPattern(1, 1);
    SetName(PLANE_A2, 0, 0, 1);
    SetName(PLANE_A2, 1, 0, 1);
    memset(hscroll_buffer_p2, 0, sizeof(hscroll_buffer_p2));
    hscroll_buffer_p2[0][0] = -8; // the second view's foreground is scrolled 8 to the right on its first line (the plane moves left: negative)
    hscroll_buffer[0][0] = 0;
    Video_UploadHScrollP2();
    CHECK_EQ(video_second_view.hscroll_location, VRAM_HSCROLL_P2);

    VDPView second = video_second_view;
    second.plane_a_location = PLANE_A2;
    VDP_SetSplitScreen(VDP_SPLIT_SIDE, &second);
    VDP_DrawFrame();
    int pitch, rows;
    const uint32_t *f = VDP_GetFrame(&pitch, &rows);
    // Right half, line 0: the foreground tile at name column 1 now sits at the view's left edge (column 0 scrolled out); line 1 is not scrolled
    CHECK_EQ(Px(f, pitch, SCREEN_WIDTH / 2, 0), PIXEL_RED);
    CHECK_EQ(Px(f, pitch, SCREEN_WIDTH / 2 + 8, 0), PIXEL_BACKDROP);
    VDP_SetSplitScreen(VDP_SPLIT_NONE, NULL);
}

void RegisterVDPSplitTests(void) {
    RUN_TEST(VDPSplit_SecondViewHasItsOwnHScrollTable);
    RUN_TEST(VDPSplit_OneViewIsAsItWas);
    RUN_TEST(VDPSplit_StackedIsTwiceTheRowsWithTallCells);
    RUN_TEST(VDPSplit_StackedSpritesAreInDoubleHeightCoordinates);
    RUN_TEST(VDPSplit_SideBySideHasEachViewItsOwnHalf);
}
