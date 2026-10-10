#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "LevelData.h"
#include "LevelDrawCore.h"
#include "LevelPlane.h"
#include "Backend/VDP.h"
#include "Viewport.h"

// The foreground plane that follows a camera (engine/LevelPlane.c), on a made-up level: every chunk of row 0 is chunk 1, whose blocks are all block 5 (tiles
// $101-$104); everything else is the empty chunk 0.

static tile_entry_t plane_memory[0x1000];
static plane_t test_plane = { plane_memory, 0x1000, NULL };

static uint16_t Word(size_t offset) {
    return Plane_Word(&test_plane, offset);
}

static void MakeLevel(void) {
    memset(level_layout, 0, sizeof(level_layout));
    for (int c = 0; c < LEVEL_LAYOUT_COLS; c++)
        LEVEL_LAYOUT_FG(0)[c] = 1;
    memset(level_map128, 0, 0x100);
    for (int i = 0; i < 64; i++) { // chunk 1's 64 block entries: block 5, no flips
        level_map128[(1 << 7) + i * 2] = 0x00;
        level_map128[(1 << 7) + i * 2 + 1] = 0x05;
    }
    static const uint8_t block5[8] = { 0x01, 0x01, 0x01, 0x02, 0x01, 0x03, 0x01, 0x04 };
    memcpy(level_map16 + 5 * 8, block5, 8);
    Plane_Clear(&test_plane);
    Viewport_SetSize(320, 224, 64, 32);
}

static void LevelPlane_DrawAllWritesTheViewsBlocks(void) {
    MakeLevel();
    dword_s cx = { 0 }, cy = { 0 };
    LevelPlane p;
    LevelPlane_Init(&p, &test_plane, &cx, &cy, 0);
    LevelPlane_DrawAll(&p, LEVEL_LAYOUT_FG(0));
    // The block at the camera's top-left corner: its four tiles, two to a row of the plane
    CHECK_EQ(Word(0), 0x0101);
    CHECK_EQ(Word(2), 0x0102);
    CHECK_EQ(Word(PLANE_WIDTH * 2 + 0), 0x0103);
    CHECK_EQ(Word(PLANE_WIDTH * 2 + 2), 0x0104);
}

static void LevelPlane_FlagsComeFromCrossingSixteenPixelLines(void) {
    dword_s cx = { 0 }, cy = { 0 };
    LevelPlane p;
    LevelPlane_Init(&p, &test_plane, &cx, &cy, 0);

    cx.f.u = 1; // the first step flags a column (the block state starts so)
    LevelPlane_CameraMovedX(&p, 0);
    CHECK_EQ(p.flags, LEVEL_SCROLL_RIGHT);
    CHECK_EQ(p.xblock, 0x10);

    LevelPlane_ClearFlags(&p);
    cx.f.u = 5; // within the same line: nothing
    LevelPlane_CameraMovedX(&p, 1);
    CHECK_EQ(p.flags, 0);
    cx.f.u = 0x11; // across the line
    LevelPlane_CameraMovedX(&p, 5);
    CHECK_EQ(p.flags, LEVEL_SCROLL_RIGHT);

    LevelPlane_ClearFlags(&p);
    cx.f.u = 0x05; // and back
    LevelPlane_CameraMovedX(&p, 0x11);
    CHECK_EQ(p.flags, LEVEL_SCROLL_LEFT);

    LevelPlane_ClearFlags(&p);
    cy.f.u = 0x10; // down across a line
    LevelPlane_CameraMovedY(&p, 0);
    CHECK_EQ(p.flags, LEVEL_SCROLL_DOWN);
    LevelPlane_ClearFlags(&p);
    cy.f.u = 0x0F; // up
    LevelPlane_CameraMovedY(&p, 0x10);
    CHECK_EQ(p.flags, LEVEL_SCROLL_UP);
}

static void LevelPlane_SnapshotThenDrawPending(void) {
    MakeLevel();
    dword_s cx = { 0 }, cy = { 0 };
    LevelPlane p;
    LevelPlane_Init(&p, &test_plane, &cx, &cy, 0);

    cx.f.u = 0x20;
    p.flags |= LEVEL_SCROLL_RIGHT;
    LevelPlane_Snapshot(&p);
    CHECK_EQ(p.flags_snap, LEVEL_SCROLL_RIGHT);
    CHECK_EQ(p.flags, LEVEL_SCROLL_RIGHT); // the live flags stay until the next frame clears them
    cx.f.u = 0x40;                          // the camera moves on: the drawing still works for the snapshot's

    size_t pos = CalcVRAMPos(0x20, 0, SCREEN_WIDTH, 0);
    CHECK_EQ(Word(pos), 0);
    LevelPlane_DrawPending(&p, LEVEL_LAYOUT_FG(0));
    CHECK_EQ(p.flags_snap, 0);
    CHECK_EQ(Word(pos), 0x0101); // the column on the right edge, for the camera as it was
}


// A made-up level of random chunks and blocks, to compare a plane scrolled step by step with one drawn afresh at the same camera
static void MakeRandomLevel(unsigned seed) {
    srand(seed);
    memset(level_layout, 0, sizeof(level_layout));
    for (int r = 0; r < LEVEL_LAYOUT_ROWS; r++)
        for (int c = 0; c < LEVEL_LAYOUT_COLS; c++)
            LEVEL_LAYOUT_FG(r)[c] = (uint8_t)(1 + rand() % 12);
    for (int i = 0; i < 0x80 * 13 / 2; i++) { // chunks 1-12: random blocks (0-63) with random flips
        uint16_t w = (uint16_t)((rand() % 64) | ((rand() & 3) << 10));
        level_map128[0x80 + i * 2] = w >> 8;
        level_map128[0x80 + i * 2 + 1] = w & 0xFF;
    }
    for (int i = 0; i < 64 * 4; i++) { // blocks 0-63: random tile words
        uint16_t w = (uint16_t)(rand() & 0x7FF);
        level_map16[i * 2] = w >> 8;
        level_map16[i * 2 + 1] = w & 0xFF;
    }
    Plane_Clear(&test_plane);
    Viewport_SetSize(320, 224, 64, 32);
}

// The cells of the view (the camera's picture) that differ from a plane drawn afresh at that camera
static int ViewMismatches(int16_t cam_x, int16_t cam_y) {
    static tile_entry_t scratch[0x1000];
    memcpy(scratch, plane_memory, sizeof(scratch)); // the plane as scrolled
    dword_s fx = { 0 }, fy = { 0 };
    fx.f.u = (uint16_t)cam_x;
    fy.f.u = (uint16_t)cam_y;
    LevelPlane fresh;
    LevelPlane_Init(&fresh, &test_plane, &fx, &fy, 0);
    LevelPlane_DrawAll(&fresh, LEVEL_LAYOUT_FG(0));
    int bad = 0;
    for (int wy = 0; wy < SCREEN_HEIGHT; wy += 8)
        for (int wx = 0; wx < SCREEN_WIDTH; wx += 8) {
            size_t cell = (size_t)((((cam_y + wy) >> 3) & (PLANE_HEIGHT - 1)) * PLANE_WIDTH + (((cam_x + wx) >> 3) & (PLANE_WIDTH - 1)));
            if (memcmp(&plane_memory[cell], &scratch[cell], sizeof(tile_entry_t)))
                bad++;
        }
    memcpy(plane_memory, scratch, sizeof(scratch)); // put the scrolled plane back, so the test goes on from it
    return bad;
}

// Scrolls through the level the way the game does -- one camera step, then the blank draws what the step flagged -- and counts the frames where the view is not what a fresh
// draw would give. max_step is the biggest camera move in a frame; frames_per_blank is how many camera steps go by between the blank's snapshots (1 in the game).
static int ScrollStress(unsigned seed, int max_step, int frames_per_blank) {
    MakeRandomLevel(seed);
    dword_s cx = { 0 }, cy = { 0 };
    cx.f.u = 0x100;
    cy.f.u = 0x100;
    LevelPlane p;
    LevelPlane_Init(&p, &test_plane, &cx, &cy, 0);
    LevelPlane_DrawAll(&p, LEVEL_LAYOUT_FG(0));
    int bad_frames = 0;
    for (int frame = 0; frame < 600; frame++) {
        for (int k = 0; k < frames_per_blank; k++) {
            int16_t ox = cx.f.u, oy = cy.f.u;
            LevelPlane_ClearFlags(&p);
            cx.f.u = (uint16_t)(ox + rand() % (2 * max_step + 1) - max_step);
            cy.f.u = (uint16_t)(oy + rand() % (2 * max_step + 1) - max_step);
            if (cx.f.u < 0x100 || cx.f.u > 0x2000) cx.f.u = (uint16_t)ox;
            if (cy.f.u < 0x100 || cy.f.u > 0x600) cy.f.u = (uint16_t)oy;
            LevelPlane_CameraMoved(&p, ox, oy);
        }
        LevelPlane_Snapshot(&p);
        LevelPlane_DrawPending(&p, LEVEL_LAYOUT_FG(0));
        if (ViewMismatches(p.snap_x.f.u, p.snap_y.f.u))
            bad_frames++;
    }
    return bad_frames;
}

// Regression test for the foreground's stale right-edge blocks: DrawBlocks_LR drew 21 blocks a row at 320 wide where the original draws 22, so the block at the right
// edge of the view was left stale in every row drawn while the camera was off a block line (only the foreground, which is what moves fast enough to show it).
static void LevelPlane_ScrollingAtGameSpeedsLeavesNoHoles(void) {
    for (unsigned seed = 1; seed <= 8; seed++)
        CHECK_EQ(ScrollStress(seed, 16, 1), 0);
}


void RegisterLevelPlaneTests(void) {
    RUN_TEST(LevelPlane_DrawAllWritesTheViewsBlocks);
    RUN_TEST(LevelPlane_FlagsComeFromCrossingSixteenPixelLines);
    RUN_TEST(LevelPlane_SnapshotThenDrawPending);
    RUN_TEST(LevelPlane_ScrollingAtGameSpeedsLeavesNoHoles);
}
