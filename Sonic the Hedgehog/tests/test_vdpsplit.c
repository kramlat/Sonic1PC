#include "test.h"

#include <string.h>

#include "Video.h"
#include "Viewport.h"
#include "Backend/VDP.h"

// The VDP's split screen (engine/Backend/VDP.c): one view alone, two stacked in the double-height mode, two side by side.

#define RED   0x000E
#define GREEN 0x00E0
#define BLUE  0x0E00
#define PIXEL_RED   0xFF0000FFu
#define PIXEL_GREEN 0x00FF00FFu
#define PIXEL_BLUE  0x0000FFFFu
#define PIXEL_BACKDROP 0x000000FFu

static void SetPattern(unsigned pattern, uint8_t colour) {
    uint8_t bytes[32];
    memset(bytes, (colour << 4) | colour, sizeof(bytes));
    VDP_SeekVRAM(pattern * 32);
    VDP_WriteVRAM(bytes, sizeof(bytes));
}

static void SetName(plane_t *plane, unsigned col, unsigned row, uint16_t entry) {
    Plane_Put(plane, (row * 64 + col) * 2, entry);
}

static void Prepare(void) {
    VDP_SetSplitScreen(VDP_SPLIT_NONE);
    VDP_ClearVRAM();
    viewport_t *both[2] = { &screen1p, &screen2p };
    for (int i = 0; i < 2; i++) {
        Viewport_UseOwnPlanes(both[i]);
        Plane_Clear(&both[i]->plane_a);
        Plane_Clear(&both[i]->plane_b);
        Viewport_ClearWindow(both[i]);
        memset(both[i]->hscroll, 0, both[i]->hscroll_bytes);
        both[i]->vsram = (vsram_t){ 0, 0 };
        both[i]->sprites = NULL;
        both[i]->palette = NULL;
    }
    Viewport_SetSize(SCREEN_WIDTH, SCREEN_HEIGHT, 64, 32);
    uint16_t cram[64];
    memset(cram, 0, sizeof(cram));
    cram[1] = RED; cram[2] = GREEN; cram[3] = BLUE;
    VDP_SeekCRAM(0);
    VDP_WriteCRAM(cram, 64);
    VDP_SetBackgroundColour(0);
    screen1p.hint_enable = false;
}

static uint32_t Px(const uint32_t *frame, int pitch, int x, int y) {
    return frame[y * pitch + x];
}

static void VDPSplit_OneViewIsAsItWas(void) {
    Prepare();
    SetPattern(1, 1);
    SetName(&screen1p.plane_a, 0, 0, 1);
    VDP_DrawFrame();
    int pitch, rows;
    const uint32_t *f = VDP_GetFrame(&pitch, &rows);
    CHECK_EQ(rows, SCREEN_HEIGHT);
    CHECK_EQ(Px(f, pitch, 0, 0), PIXEL_RED);
    CHECK_EQ(Px(f, pitch, 7, 7), PIXEL_RED);
    CHECK_EQ(Px(f, pitch, 8, 0), PIXEL_BACKDROP);
    CHECK_EQ(Px(f, pitch, 0, 8), PIXEL_BACKDROP);
}

static void VDPSplit_StackedSquashesTwoWholeViewsIntoHalvesOfThePicture(void) {
    Prepare();
    SetPattern(1, 1);
    SetPattern(2, 3);
    SetName(&screen1p.plane_a, 0, 0, 1);
    SetName(&screen2p.plane_a, 0, 0, 2);

    VDP_SetSplitScreen(VDP_SPLIT_STACKED);
    CHECK_EQ(VDP_OutputRows(), SCREEN_HEIGHT);

    VDP_DrawFrame();
    int pitch, rows;
    const uint32_t *f = VDP_GetFrame(&pitch, &rows);
    CHECK_EQ(rows, SCREEN_HEIGHT);
    // The first view, an ordinary one (8 lines for a cell), at half height: 4 lines
    CHECK_EQ(Px(f, pitch, 0, 0), PIXEL_RED);
    CHECK_EQ(Px(f, pitch, 0, 3), PIXEL_RED);
    CHECK_EQ(Px(f, pitch, 0, 4), PIXEL_BACKDROP);
    // The second view starts half the picture down, with its own plane
    CHECK_EQ(Px(f, pitch, 0, SCREEN_HEIGHT / 2), PIXEL_BLUE);
    CHECK_EQ(Px(f, pitch, 0, SCREEN_HEIGHT / 2 + 3), PIXEL_BLUE);
    CHECK_EQ(Px(f, pitch, 0, SCREEN_HEIGHT / 2 + 4), PIXEL_BACKDROP);
    VDP_SetSplitScreen(VDP_SPLIT_NONE);
}

static void VDPSplit_StackedSpritesAreOrdinaryOnesInEachHalf(void) {
    Prepare();
    SetPattern(4, 3);
    SetPattern(6, 2);
    static sprite_t table1[2], table2[2];
    memset(table1, 0, sizeof(table1));
    memset(table2, 0, sizeof(table2));
    // One cell (width 0, height 0) at x 10, line 5 of the first view; the second view's sprite at line 3 of its own
    table1[0] = (sprite_t){ 128 + 5, 0x0000, 4, 128 + 10, 0, 0 };
    table2[0] = (sprite_t){ 128 + 3, 0x0000, 6, 128 + 20, 0, 0 };
    screen1p.sprites = table1;
    screen2p.sprites = table2;
    VDP_SetSplitScreen(VDP_SPLIT_STACKED);
    VDP_DrawFrame();
    int pitch, rows;
    const uint32_t *f = VDP_GetFrame(&pitch, &rows);
    CHECK_EQ(Px(f, pitch, 10, 0), PIXEL_BACKDROP);
    CHECK_EQ(Px(f, pitch, 10, 3), PIXEL_BLUE);        // lines 6-7 of the view, both inside the sprite
    CHECK_EQ(Px(f, pitch, 18, 3), PIXEL_BACKDROP);    // 8 wide
    CHECK_EQ(Px(f, pitch, 20, SCREEN_HEIGHT / 2), PIXEL_BACKDROP);
    CHECK_EQ(Px(f, pitch, 20, SCREEN_HEIGHT / 2 + 3), PIXEL_GREEN); // the second view's sprite, in its own half
    CHECK_EQ(Px(f, pitch, 20, 3), PIXEL_BACKDROP);                  // not in the first
    VDP_SetSplitScreen(VDP_SPLIT_NONE);
    screen1p.sprites = NULL;
    screen2p.sprites = NULL;
}

static void VDPSplit_SideBySideHasEachViewItsOwnHalf(void) {
    Prepare();
    SetPattern(1, 1);
    SetPattern(2, 1);
    SetName(&screen1p.plane_a, 0, 0, 1);
    SetName(&screen2p.plane_a, 0, 0, 2);
    uint16_t cram2[64];
    memset(cram2, 0, sizeof(cram2));
    cram2[1] = BLUE; // the second view shows colour 1 as blue: its own palette
    screen2p.palette = cram2;
    VDP_SetSplitScreen(VDP_SPLIT_SIDE);
    CHECK_EQ(VDP_OutputRows(), SCREEN_HEIGHT);
    VDP_DrawFrame();
    int pitch, rows;
    const uint32_t *f = VDP_GetFrame(&pitch, &rows);
    CHECK_EQ(Px(f, pitch, 0, 0), PIXEL_RED);                          // the left half: the first view
    CHECK_EQ(Px(f, pitch, SCREEN_WIDTH / 2 - 1, 0), PIXEL_BACKDROP);
    CHECK_EQ(Px(f, pitch, SCREEN_WIDTH / 2, 0), PIXEL_BLUE);          // the right half: the second view's plane and palette, from its own left edge
    CHECK_EQ(Px(f, pitch, SCREEN_WIDTH / 2 + 7, 7), PIXEL_BLUE);
    CHECK_EQ(Px(f, pitch, SCREEN_WIDTH / 2 + 8, 0), PIXEL_BACKDROP);
    VDP_SetSplitScreen(VDP_SPLIT_NONE);
}

static void VDPSplit_SecondViewHasItsOwnHScrollTable(void) {
    Prepare();
    SetPattern(1, 1);
    SetName(&screen2p.plane_a, 0, 0, 1);
    SetName(&screen2p.plane_a, 1, 0, 1);
    memset(hscroll_buffer_p2, 0, sizeof(hscroll_buffer_p2));
    hscroll_buffer_p2[0][0] = -8; // the second view's foreground is scrolled 8 to the right on its first line (the plane moves left: negative)
    hscroll_buffer[0][0] = 0;
    Video_UploadHScrollP2();
    VDP_SetSplitScreen(VDP_SPLIT_SIDE);
    VDP_DrawFrame();
    int pitch, rows;
    const uint32_t *f = VDP_GetFrame(&pitch, &rows);
    // Right half, line 0: the foreground tile at name column 1 now sits at the view's left edge (column 0 scrolled out); line 1 is not scrolled
    CHECK_EQ(Px(f, pitch, SCREEN_WIDTH / 2, 0), PIXEL_RED);
    CHECK_EQ(Px(f, pitch, SCREEN_WIDTH / 2 + 8, 0), PIXEL_BACKDROP);
    VDP_SetSplitScreen(VDP_SPLIT_NONE);
}

void RegisterVDPSplitTests(void) {
    RUN_TEST(VDPSplit_SecondViewHasItsOwnHScrollTable);
    RUN_TEST(VDPSplit_OneViewIsAsItWas);
    RUN_TEST(VDPSplit_StackedSquashesTwoWholeViewsIntoHalvesOfThePicture);
    RUN_TEST(VDPSplit_StackedSpritesAreOrdinaryOnesInEachHalf);
    RUN_TEST(VDPSplit_SideBySideHasEachViewItsOwnHalf);
}
