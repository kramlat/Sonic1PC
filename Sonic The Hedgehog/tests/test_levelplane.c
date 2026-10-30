#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "LevelData.h"
#include "Kosinski.h"
#include "Level.h"
#include "LevelLayout.h"
#include "ScrollEngine.h"
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
    LevelLayout_Resize(LEVEL_LAYOUT_PLANE_FG, 0, 0);
    for (int c = 0; c < LEVEL_LAYOUT_COLS; c++)
        LevelLayout_SetChunk(LEVEL_LAYOUT_PLANE_FG, c, 0, 1);
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
    LevelPlane_DrawAll(&p, LEVEL_LAYOUT_PLANE_FG);
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
    LevelPlane_DrawPending(&p, LEVEL_LAYOUT_PLANE_FG);
    CHECK_EQ(p.flags_snap, 0);
    CHECK_EQ(Word(pos), 0x0101); // the column on the right edge, for the camera as it was
}


// A made-up level of random chunks and blocks, to compare a plane scrolled step by step with one drawn afresh at the same camera
static void MakeRandomLevel(unsigned seed) {
    srand(seed);
    LevelLayout_Resize(LEVEL_LAYOUT_PLANE_FG, 0, 0);
    for (int r = 0; r < LEVEL_LAYOUT_ROWS; r++)
        for (int c = 0; c < LEVEL_LAYOUT_COLS; c++)
            LevelLayout_SetChunk(LEVEL_LAYOUT_PLANE_FG, c, r, (uint8_t)(1 + rand() % 12));
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
    LevelPlane_DrawAll(&fresh, LEVEL_LAYOUT_PLANE_FG);
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
    LevelPlane_DrawAll(&p, LEVEL_LAYOUT_PLANE_FG);
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
        LevelPlane_DrawPending(&p, LEVEL_LAYOUT_PLANE_FG);
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


// Each level's layout planes are exactly as big as its file says and read the same as the old fixed buffer did (the unpacked file, 16 rows of 128 chunks, 0 where the level has nothing)
static void LevelLayout_PlanesAreExactAndReadLikeTheFixedBuffer(void) {
    static uint8_t file[0x4000], reference[LEVEL_LAYOUT_ROWS * 0x100];
    int levels = 0, mismatches = 0;
    for (int zone = 0; zone < ZoneId_Num; zone++)
        for (int act = 0; act < 4; act++) {
            const uint8_t *blob = level_layouts[zone][act].layout;
            if (blob == NULL)
                continue;
            LevelLayoutSize size;
            CHECK(LevelLayout_Unpack(file, KosDec(blob, file) - file, reference, LEVEL_LAYOUT_ROWS, &size));
            CHECK(LevelLayout_LoadKosinski(blob));
            CHECK_EQ(level_layout_fg.width, size.fg_width);
            CHECK_EQ(level_layout_fg.rows, size.fg_rows);
            CHECK_EQ(level_layout_bg.width, size.bg_width);
            CHECK_EQ(level_layout_bg.rows, size.bg_rows);
            for (int y = 0; y < LEVEL_LAYOUT_ROWS; y++)
                for (int x = 0; x < LEVEL_LAYOUT_COLS; x++) {
                    mismatches += LevelLayout_Chunk(LEVEL_LAYOUT_PLANE_FG, x, y) != reference[y * 0x100 + x];
                    mismatches += LevelLayout_Chunk(LEVEL_LAYOUT_PLANE_BG, x, y) != reference[y * 0x100 + 0x80 + x];
                }
            levels++;
        }
    CHECK(levels > 10);
    CHECK_EQ(mismatches, 0);
}

// --- The scrolling engine's zone routines and a level's wrap masks ---------------

static char scroll_trace[16];
static int scroll_trace_n;
static void TraceA(void) { scroll_trace[scroll_trace_n++] = 'a'; }
static void TraceB(void) { scroll_trace[scroll_trace_n++] = 'b'; }
static void TraceC(void) { scroll_trace[scroll_trace_n++] = 'c'; }
static void TraceD(void) { scroll_trace[scroll_trace_n++] = 'd'; }

static void ScrollEngine_RunsAZonesRoutinesInTheOriginalsOrder(void) {
    static const ScrollZone zone = { TraceA, TraceB, TraceC, TraceD };
    ScrollEngine_SetZone(51, -1, &zone); // (zones no game has: 51 to 53)
    ScrollEngine_SetZone(52, 2, &(ScrollZone){ .background_event = TraceD }); // (an act of its own, and no other routines)
    scroll_trace_n = 0;
    ScrollEngine_Init(51, 1);
    CHECK_EQ(scroll_trace_n, 2);
    CHECK(scroll_trace[0] == 'a' && scroll_trace[1] == 'b'); // (the foreground's init, then the background's)
    scroll_trace_n = 0;
    ScrollEngine_Events(51, 3);
    CHECK_EQ(scroll_trace_n, 2);
    CHECK(scroll_trace[0] == 'c' && scroll_trace[1] == 'd'); // (the foreground's event, then the background's)
    scroll_trace_n = 0;
    ScrollEngine_Events(52, 2);
    ScrollEngine_Events(52, 1); // (the zone's other act has nothing)
    ScrollEngine_Events(53, 0); // (nor has a zone that was never set)
    CHECK_EQ(scroll_trace_n, 1);
    CHECK(ScrollEngine_GetZone(52, 1) == NULL);
}

static void ScrollEngine_ALevelsMasksSetHowItWraps(void) {
    LevelLayout_Resize(LEVEL_LAYOUT_PLANE_FG, 0, 0);
    LevelLayout_SetChunk(LEVEL_LAYOUT_PLANE_FG, 3, 2, 9);
    CHECK_EQ(LevelLayout_Chunk(LEVEL_LAYOUT_PLANE_FG, 3 + 0x80, 2 + 16), 9); // (the engine's wrap: 128 across and 16 down)
    ScrollEngine_SetLevelMasks(0x3F, 7);
    CHECK_EQ(level_screen_y_wrap, 0x3FF);                                   // (8 rows of 128 pixels)
    CHECK_EQ(level_camera_y_mask, 0x3F0);
    CHECK_EQ(LevelLayout_Chunk(LEVEL_LAYOUT_PLANE_FG, 3 + 0x40, 2 + 8), 9); // (64 across and 8 down now)
    CHECK_EQ(LevelLayout_Chunk(LEVEL_LAYOUT_PLANE_FG, 3 + 0x80, 2 + 16), 9);
    ScrollEngine_SetLevelMasks(0x7F, 0xF); // (the defaults back)
    CHECK_EQ(level_screen_y_wrap, 0x7FF);
    CHECK_EQ(level_camera_y_mask, 0x7F0);
    LevelLayout_Resize(LEVEL_LAYOUT_PLANE_FG, 0, 0);
}


// ---- band tables (engine/ScrollEngine.c) ----

static void ScrollBands_FindTheBandAndTheLineInIt(void) {
    static const uint16_t bands[] = { 32, 16, 64 };
    int off;
    CHECK_EQ(ScrollBands_BandAt(bands, 3, 0, &off), 0);
    CHECK_EQ(off, 0);
    CHECK_EQ(ScrollBands_BandAt(bands, 3, 31, &off), 0);
    CHECK_EQ(off, 31);
    CHECK_EQ(ScrollBands_BandAt(bands, 3, 32, &off), 1);
    CHECK_EQ(off, 0);
    CHECK_EQ(ScrollBands_BandAt(bands, 3, 60, &off), 2);
    CHECK_EQ(off, 12);
    CHECK_EQ(ScrollBands_BandAt(bands, 3, 500, 0), 2); // (below the table: the last band)
    CHECK_EQ(ScrollBands_BandAt(bands, 3, -9, 0), 0);
}

static void ScrollEngine_ApplyDeformationFillsTheLinesLikeTheOriginal(void) {
    // Bands of 4, 2 and 3 lines (the middle one with a position for every line): positions 100 | 10, 20 | 7
    static const uint16_t bands[] = { 4, 2 | SCROLL_BAND_PER_LINE, 3 };
    static const int16_t pos[] = { 100, 10, 20, 7 };
    int16_t out[12][2];
    ScrollEngine_ApplyDeformation(bands, 3, pos, 0, 55, true, 9, out);
    static const int16_t want_bg[9] = { -100, -100, -100, -100, -10, -20, -7, -7, -7 };
    for (int i = 0; i < 9; i++) {
        CHECK_EQ(out[i][1], want_bg[i]);
        CHECK_EQ(out[i][0], -55); // (the foreground's camera)
    }
    // Starting inside the bands: layer line 3 is the last line of the first band, line 5 the second of the per-line band
    ScrollEngine_ApplyDeformation(bands, 3, pos, 3, 55, true, 3, out);
    CHECK_EQ(out[0][1], -100);
    CHECK_EQ(out[1][1], -10);
    CHECK_EQ(out[2][1], -20);
    ScrollEngine_ApplyDeformation(bands, 3, pos, 5, 55, true, 2, out);
    CHECK_EQ(out[0][1], -20);
    CHECK_EQ(out[1][1], -7);
    // The table ends before the lines do: the last position holds
    ScrollEngine_ApplyDeformation(bands, 3, pos, 6, 55, true, 12, out);
    for (int i = 0; i < 12; i++)
        CHECK_EQ(out[i][1], -7);
    // A deformed foreground (ApplyFGDeformation): the bands are the foreground's, the other value is the background's camera
    ScrollEngine_ApplyDeformation(bands, 3, pos, 0, 33, false, 5, out);
    CHECK_EQ(out[0][0], -100);
    CHECK_EQ(out[4][0], -10);
    CHECK_EQ(out[4][1], -33);
}

// A made-up level whose every block differs: chunk (c, r) is 1 + (c + r * 7) % 200, and each block entry in it its own block
static tile_entry_t band_memory[2][0x1000];
static plane_t band_plane[2] = { { band_memory[0], 0x1000, NULL }, { band_memory[1], 0x1000, NULL } };

static void MakeDistinctLevel(void) {
    LevelLayout_Resize(LEVEL_LAYOUT_PLANE_BG, 0, 0);
    for (int r = 0; r < 16; r++)
        for (int c = 0; c < 128; c++)
            LevelLayout_SetChunk(LEVEL_LAYOUT_PLANE_BG, c, r, (uint8_t)(1 + (c + r * 7) % 200));
    for (int chunk = 0; chunk < 256; chunk++)
        for (int i = 0; i < 64; i++) {
            unsigned blk = (unsigned)(chunk * 64 + i) & 0x3FF;
            level_map128[(chunk << 7) + i * 2] = (uint8_t)(blk >> 8);
            level_map128[(chunk << 7) + i * 2 + 1] = (uint8_t)blk;
        }
    for (unsigned blk = 0; blk < 0x400; blk++)
        for (int k = 0; k < 4; k++) {
            unsigned w = (blk * 4 + k) & 0x7FF;
            level_map16[blk * 8 + k * 2] = (uint8_t)(w >> 8);
            level_map16[blk * 8 + k * 2 + 1] = (uint8_t)w;
        }
    Viewport_SetSize(320, 224, 64, 32);
}

// The view's cells of both planes agree: for every row drawn, the columns from one block left of the view to the right edge, at their own band's X
static bool BandPlanesAgree(const ScrollBands *a, const ScrollBands *b) {
    int first = (a->y >> 4) - 1;
    for (int r = first; r < first + SCROLL_ROWS; r++) {
        int band = ScrollBands_BandAt(a->heights, a->count, r * 16, 0);
        int c0 = (a->x[band] >> 4) - 1;
        for (int c = c0; c <= (a->x[band] >> 4) + RIGHT_EDGE_X / 16; c++) {
            size_t pos = CalcVRAMPos((int16_t)(c * 16), 0, 0, (int16_t)(r * 16));
            size_t cell[4] = { pos, pos + 2, pos + PLANE_WIDTH * 2, pos + PLANE_WIDTH * 2 + 2 };
            for (int k = 0; k < 4; k++)
                if (Plane_Word(a->plane, cell[k]) != Plane_Word(b->plane, cell[k]))
                    return false;
        }
    }
    return true;
}

static void ScrollBands_MovingBandsDrawLikeDrawingTheViewAgain(void) {
    MakeDistinctLevel();
    static const uint16_t bands[] = { 48, 16, 96, 32, 64, 128 };
    ScrollBands moving, fresh;
    ScrollBands_Init(&moving, &band_plane[0], bands, 6);
    ScrollBands_Init(&fresh, &band_plane[1], bands, 6);
    Plane_Clear(&band_plane[0]);
    for (int b = 0; b < 6; b++)
        moving.x[b] = (int16_t)(200 + b * 37);
    moving.y = 300;
    ScrollBands_DrawAll(&moving, LEVEL_LAYOUT_PLANE_BG);

    unsigned seed = 12345;
    for (int step = 0; step < 600; step++) {
        seed = seed * 1103515245u + 12345u;
        int speed = (int)((seed >> 16) % 5) + 1; // 1..5 pixels a frame, the bands each at a different rate
        int dir = (seed >> 24) & 1 ? 1 : -1;
        for (int b = 0; b < 6; b++)
            moving.x[b] = (int16_t)(moving.x[b] + dir * ((speed * (b + 1)) / 2));
        if (step % 3 == 0)
            moving.y = (int16_t)(moving.y + (((seed >> 8) & 1) ? 2 : -3));
        if (step == 300) // (a jump: more than the plane holds)
            moving.y = 900;
        ScrollBands_Draw(&moving, LEVEL_LAYOUT_PLANE_BG);

        Plane_Clear(&band_plane[1]);
        memcpy(fresh.x, moving.x, sizeof(fresh.x));
        fresh.y = moving.y;
        ScrollBands_DrawAll(&fresh, LEVEL_LAYOUT_PLANE_BG);
        if (!BandPlanesAgree(&moving, &fresh)) {
            CHECK_EQ(step, -1); // (fails, naming the step)
            break;
        }
    }
    CHECK(true);
    LevelLayout_Resize(LEVEL_LAYOUT_PLANE_BG, 0, 0);
}

void RegisterLevelPlaneTests(void) {
    RUN_TEST(ScrollEngine_RunsAZonesRoutinesInTheOriginalsOrder);
    RUN_TEST(ScrollEngine_ALevelsMasksSetHowItWraps);
    RUN_TEST(ScrollBands_FindTheBandAndTheLineInIt);
    RUN_TEST(ScrollEngine_ApplyDeformationFillsTheLinesLikeTheOriginal);
    RUN_TEST(ScrollBands_MovingBandsDrawLikeDrawingTheViewAgain);
    RUN_TEST(LevelLayout_PlanesAreExactAndReadLikeTheFixedBuffer);
    RUN_TEST(LevelPlane_DrawAllWritesTheViewsBlocks);
    RUN_TEST(LevelPlane_FlagsComeFromCrossingSixteenPixelLines);
    RUN_TEST(LevelPlane_SnapshotThenDrawPending);
    RUN_TEST(LevelPlane_ScrollingAtGameSpeedsLeavesNoHoles);
}
