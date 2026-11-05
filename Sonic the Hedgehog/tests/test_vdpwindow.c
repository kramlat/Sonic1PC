#include "test.h"

#include <string.h>

#include "Video.h"
#include "Viewport.h"
#include "Backend/VDP.h"

// The window plane (engine/Backend/VDP.c): where its region is, plane A is replaced by a plane that does not scroll; plane B shows through its transparent pixels.

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

static void Prepare(void) {
    VDP_SetSplitScreen(VDP_SPLIT_NONE);
    VDP_ClearVRAM();
    Viewport_UseOwnPlanes(&screen1p);
    Plane_Clear(&screen1p.plane_a);
    Plane_Clear(&screen1p.plane_b);
    Plane_Clear(&screen1p.window);
    Viewport_ClearWindow(&screen1p);
    memset(screen1p.hscroll, 0, screen1p.hscroll_bytes);
    screen1p.vsram = (vsram_t){ 0, 0 };
    screen1p.sprites = NULL;
    Viewport_SetSize(SCREEN_WIDTH, SCREEN_HEIGHT, 64, 32);
    uint16_t cram[64];
    memset(cram, 0, sizeof(cram));
    cram[1] = 0x000E; cram[2] = 0x00E0; cram[3] = 0x0E00; // red, green, blue
    VDP_SeekCRAM(0);
    VDP_WriteCRAM(cram, 64);
    VDP_SetBackgroundColour(0);
    screen1p.hint_enable = false;
    SetPattern(1, 1); // red
    SetPattern(2, 2); // green
    SetPattern(3, 3); // blue
}

static void Put(plane_t *plane, unsigned col, unsigned row, uint16_t word) {
    Plane_Put(plane, (row * 64 + col) * 2, word);
}

static const uint32_t *Frame(int *pitch) {
    VDP_DrawFrame();
    int rows;
    return VDP_GetFrame(pitch, &rows);
}

// Plane A is red all over the first rows, the window green; plane B blue
static void Fill(void) {
    for (unsigned r = 0; r < 8; r++)
        for (unsigned c = 0; c < 64; c++) {
            Put(&screen1p.plane_a, c, r, 1);
            Put(&screen1p.window, c, r, 2);
            Put(&screen1p.plane_b, c, r, 3);
        }
}

static void VDPWindow_NoRegionNoWindow(void) {
    Prepare();
    Fill();
    int pitch;
    const uint32_t *f = Frame(&pitch);
    CHECK_EQ(f[0], PIXEL_RED);
    CHECK_EQ(f[100 * 0 + SCREEN_WIDTH - 1], PIXEL_RED);
}

static void VDPWindow_AnEdgeFromTheLeftCoversTheLeftOfTheRow(void) {
    Prepare();
    Fill();
    Viewport_SetWindow(&screen1p, (window_region_t){ .use_x = true, .x = 16, .right = false });
    int pitch;
    const uint32_t *f = Frame(&pitch);
    CHECK_EQ(f[0], PIXEL_GREEN);
    CHECK_EQ(f[15], PIXEL_GREEN);
    CHECK_EQ(f[16], PIXEL_RED);
}

static void VDPWindow_AnEdgeFromTheRightCoversTheRight(void) {
    Prepare();
    Fill();
    Viewport_SetWindow(&screen1p, (window_region_t){ .use_x = true, .x = SCREEN_WIDTH - 16, .right = true });
    int pitch;
    const uint32_t *f = Frame(&pitch);
    CHECK_EQ(f[SCREEN_WIDTH - 17], PIXEL_RED);
    CHECK_EQ(f[SCREEN_WIDTH - 16], PIXEL_GREEN);
    CHECK_EQ(f[SCREEN_WIDTH - 1], PIXEL_GREEN);
}

static void VDPWindow_AnEdgeFromTheTopCoversTheTopRows(void) {
    Prepare();
    Fill();
    Viewport_SetWindow(&screen1p, (window_region_t){ .use_y = true, .y = 8, .below = false });
    int pitch;
    const uint32_t *f = Frame(&pitch);
    CHECK_EQ(f[7 * pitch + 50], PIXEL_GREEN);
    CHECK_EQ(f[8 * pitch + 50], PIXEL_RED);
}

static void VDPWindow_BothEdgesGiveBothParts(void) {
    Prepare();
    Fill();
    Viewport_SetWindow(&screen1p, (window_region_t){ .use_x = true, .x = 8, .right = false, .use_y = true, .y = 8, .below = false });
    int pitch;
    const uint32_t *f = Frame(&pitch);
    CHECK_EQ(f[3 * pitch + 100], PIXEL_GREEN); // above the y edge: the whole row
    CHECK_EQ(f[12 * pitch + 3], PIXEL_GREEN);  // below it, left of the x edge
    CHECK_EQ(f[12 * pitch + 100], PIXEL_RED);  // below it and right of it: plane A
}

static void VDPWindow_DoesNotScrollAndPlaneBShowsThroughItsClearPixels(void) {
    Prepare();
    Fill();
    Put(&screen1p.window, 0, 0, 0); // a clear tile in the window: plane B shows, not plane A
    for (int i = 0; i < SCREEN_MAX_HEIGHT; i++) {
        screen1p.hscroll[i * 2] = -24; // plane A scrolls
    }
    screen1p.vsram.a = 0;
    Viewport_SetWindow(&screen1p, (window_region_t){ .use_x = true, .x = 16, .right = false });
    int pitch;
    const uint32_t *f = Frame(&pitch);
    CHECK_EQ(f[0], PIXEL_BLUE);   // the window's clear tile: plane B
    CHECK_EQ(f[8], PIXEL_GREEN);  // the window's next tile, where it is (not scrolled)
    CHECK_EQ(f[16], PIXEL_RED);   // plane A beyond the edge
}

static void VDPWindow_ClearingItTakesItAway(void) {
    Prepare();
    Fill();
    Viewport_SetWindow(&screen1p, (window_region_t){ .use_y = true, .y = 8, .below = false });
    Viewport_ClearWindow(&screen1p);
    int pitch;
    const uint32_t *f = Frame(&pitch);
    CHECK_EQ(f[0], PIXEL_RED);
}

void RegisterVDPWindowTests(void) {
    RUN_TEST(VDPWindow_ClearingItTakesItAway);
    RUN_TEST(VDPWindow_NoRegionNoWindow);
    RUN_TEST(VDPWindow_AnEdgeFromTheLeftCoversTheLeftOfTheRow);
    RUN_TEST(VDPWindow_AnEdgeFromTheRightCoversTheRight);
    RUN_TEST(VDPWindow_AnEdgeFromTheTopCoversTheTopRows);
    RUN_TEST(VDPWindow_BothEdgesGiveBothParts);
    RUN_TEST(VDPWindow_DoesNotScrollAndPlaneBShowsThroughItsClearPixels);
}
