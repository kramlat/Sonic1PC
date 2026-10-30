#include "test.h"

#include "Camera.h"
#include "LevelScroll.h"
#include "Video.h"
#include "Level.h"
#include "ScrollEngine.h"
#include "LevelDraw.h"
#include "Backend/VDP.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The prototype's Scroll_Block1..4 (and the shared UpdateBGScroll the port's other background scrolls use): a background moves by a 16.16 amount, and when it crosses a line between 16 pixel blocks its
// "crossed" tracker toggles and the zone's flag for the direction is raised, so the level drawing adds the row or column that has come into view. Left raises the flag's bit and right the next one (D6 and
// D6 + 1 in the original); a move inside a block raises nothing.

extern uint8_t bg1_yblock, bg2_yblock, bg3_yblock;
extern void BGScroll_Block2(int32_t x, uint8_t bit);
extern void BGScroll_Block3(int32_t x, uint8_t bit);
extern void UpdateBGScroll(dword_s *pos, int32_t delta, uint8_t *block, uint16_t *flags, uint16_t neg_bit, uint16_t pos_bit);
extern void BGScroll_Y(int32_t y_off);
extern void BGScroll_YRelative(int32_t y_off);

#define PX(n) ((int32_t)(n) << 16)

static void Reset(void) {
    bg_scrpos_x.v = bg_scrpos_y.v = bg2_scrpos_x.v = bg3_scrpos_x.v = 0;
    bg1_xblock = bg2_xblock = bg3_xblock = 0x10; // (the block bit the position is not in: nothing is crossed until it is)
    bg1_yblock = 0;
    bg1_scroll_flags = bg2_scroll_flags = bg3_scroll_flags = 0;
}

static void ScrollBlock_RightAcrossALineRaisesTheRightFlag(void) {
    Reset();
    BGScroll_Block1(PX(0x10), SCROLL_FLAG_LEFT);
    CHECK_EQ(bg1_scroll_flags, SCROLL_FLAG_RIGHT);
    CHECK_EQ(bg1_xblock, 0); // (the tracker toggled)
}

static void ScrollBlock_LeftAcrossALineRaisesTheLeftFlag(void) {
    Reset();
    bg_scrpos_x.v = PX(0x10);
    bg1_xblock = 0; // (not the position's block bit, 0x10: quiet until the position's bit is the tracker's)
    BGScroll_Block1(-PX(1), SCROLL_FLAG_LEFT); // to $0F: the bit is clear
    CHECK_EQ(bg1_scroll_flags, SCROLL_FLAG_LEFT);
    CHECK_EQ(bg1_xblock, 0x10);
}

static void ScrollBlock_AMoveInsideABlockRaisesNothing(void) {
    Reset();
    BGScroll_Block1(PX(5), SCROLL_FLAG_LEFT);
    BGScroll_Block1(PX(5), SCROLL_FLAG_LEFT);
    CHECK_EQ(bg1_scroll_flags, 0);
    CHECK_EQ(bg1_xblock, 0x10);
}

static void ScrollBlock_NoMoveRaisesNothing(void) {
    Reset();
    BGScroll_Block1(0, SCROLL_FLAG_LEFT);
    CHECK_EQ(bg1_scroll_flags, 0);
}

// One pixel at a time: the line is crossed once, at the 16th pixel
static void ScrollBlock_SingleStepsCrossTheLineOnce(void) {
    Reset();
    int crossed = 0, at = -1;
    for (int i = 1; i <= 15; i++) {
        BGScroll_Block1(PX(1), SCROLL_FLAG_LEFT);
        if (bg1_scroll_flags) {
            crossed++;
            at = i;
            bg1_scroll_flags = 0;
        }
    }
    CHECK_EQ(crossed, 0);
    BGScroll_Block1(PX(1), SCROLL_FLAG_LEFT);
    CHECK_EQ(bg1_scroll_flags, SCROLL_FLAG_RIGHT);
    CHECK_EQ(at, -1);
}

// Fractions add up: sixteen steps of a sixteenth of a pixel are no move, and the sub-pixel part of the position is kept
static void ScrollBlock_FractionsAccumulate(void) {
    Reset();
    for (int i = 0; i < 16; i++)
        BGScroll_Block1(0x1000, SCROLL_FLAG_LEFT); // 1/16 pixel
    CHECK_EQ(bg_scrpos_x.f.u, 1);
    CHECK_EQ(bg1_scroll_flags, 0);
}

// The flags of a zone are ORed, not replaced (several scrolls in one frame)
static void ScrollBlock_FlagsAccumulate(void) {
    Reset();
    bg1_scroll_flags = SCROLL_FLAG_UP;
    BGScroll_Block1(PX(0x10), SCROLL_FLAG_LEFT);
    CHECK_EQ(bg1_scroll_flags, SCROLL_FLAG_UP | SCROLL_FLAG_RIGHT);
}

// The flag bit is the caller's: bit and bit + 1 (the prototype's D6 and D6 + 1: block 2's bit 4 / 5 and, for Hill Top's y, bits 6 / 7)
static void ScrollBlock_TheFlagBitIsTheCallers(void) {
    Reset();
    BGScroll_Block1(PX(0x10), SCROLL_FLAG_UP2);
    CHECK_EQ(bg1_scroll_flags, SCROLL_FLAG_DOWN2);
    Reset();
    BGScroll_Block1(PX(0x10), 1 << 6);
    CHECK_EQ(bg1_scroll_flags, 1 << 7);
}

// Blocks 2 and 3 have their own position, tracker and flags
static void ScrollBlock_TheThreeBlocksAreIndependent(void) {
    Reset();
    BGScroll_Block2(PX(0x10), SCROLL_FLAG_LEFT);
    CHECK_EQ(bg2_scroll_flags, SCROLL_FLAG_RIGHT);
    CHECK_EQ(bg2_xblock, 0);
    CHECK_EQ(bg2_scrpos_x.f.u, 0x10);
    CHECK_EQ(bg1_scroll_flags, 0);
    CHECK_EQ(bg1_xblock, 0x10);
    CHECK_EQ(bg_scrpos_x.f.u, 0);
    BGScroll_Block3(PX(0x10), SCROLL_FLAG_LEFT);
    CHECK_EQ(bg3_scroll_flags, SCROLL_FLAG_RIGHT);
    CHECK_EQ(bg3_scrpos_x.f.u, 0x10);
    CHECK_EQ(bg1_scroll_flags, 0);
    CHECK_EQ(bg2_scroll_flags, SCROLL_FLAG_RIGHT);
}

// --- UpdateBGScroll, the same for either axis ---

static void ScrollBlock_UpdateRaisesTheBitForTheDirectionMoved(void) {
    Reset();
    uint8_t block = 0;
    uint16_t flags = 0;
    dword_s pos = { 0 };
    UpdateBGScroll(&pos, PX(0x10), &block, &flags, 1 << 0, 1 << 1);
    CHECK_EQ(flags, 1 << 1);
    CHECK_EQ(block, 0x10);
    CHECK_EQ(pos.f.u, 0x10);

    flags = 0;
    UpdateBGScroll(&pos, -PX(0x10), &block, &flags, 1 << 0, 1 << 1);
    CHECK_EQ(flags, 1 << 0); // (back across the line)
    CHECK_EQ(block, 0);
    CHECK_EQ(pos.f.u, 0);
}

static void ScrollBlock_UpdateInsideABlockRaisesNothing(void) {
    uint8_t block = 0;
    uint16_t flags = 0;
    dword_s pos = { 0 };
    UpdateBGScroll(&pos, PX(15), &block, &flags, 1 << 0, 1 << 1);
    CHECK_EQ(flags, 0);
    CHECK_EQ(block, 0);
}

// The two ways of tracking a block (Block1's toggles when the tracker is the position's bit, UpdateBGScroll's when it is not) have the trackers opposite and then raise the same flags on the same frames
static void ScrollBlock_BothTrackersAgreeAcrossASweep(void) {
    Reset();
    uint8_t block = 0;
    uint16_t flags = 0;
    dword_s pos = { 0 };
    static const int steps[] = { 3, 5, -2, 7, -9, 16, -16, 1, 1, 1, 31, -40, 13, 13, 13, -7, -7 };
    for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
        bg1_scroll_flags = 0;
        flags = 0;
        BGScroll_Block1(PX(steps[i]), SCROLL_FLAG_LEFT);
        UpdateBGScroll(&pos, PX(steps[i]), &block, &flags, SCROLL_FLAG_LEFT, SCROLL_FLAG_RIGHT);
        CHECK_EQ(bg1_scroll_flags, flags);
        CHECK_EQ(bg_scrpos_x.f.u, pos.f.u);
    }
}

// --- the callers' helpers ---

static void ScrollBlock_XYUsesTheFirstFlagsForBothAxes(void) {
    Reset();
    bg1_xblock = 0; // (this one's trackers are UpdateBGScroll's: they toggle when the position's bit is not the tracker's)
    bg1_yblock = 0;
    BGScroll_XY(PX(0x10), PX(0x10));
    CHECK_EQ(bg1_scroll_flags, SCROLL_FLAG_RIGHT | SCROLL_FLAG_DOWN);
}

static void ScrollBlock_YUsesTheSecondRowOfFlags(void) {
    Reset();
    BGScroll_Y(PX(0x10));
    CHECK_EQ(bg1_scroll_flags, SCROLL_FLAG_DOWN2);
    Reset();
    BGScroll_YRelative(PX(0x10));
    CHECK_EQ(bg1_scroll_flags, SCROLL_FLAG_DOWN);
}

extern void Deform_EHZ(void);
extern void Deform_TitleScreen(void);

// Emerald Hill's scroll lines (sky, clouds, the wavy line, hills, the water's gradient in steps of one, two and three lines) over a sweep of camera positions, the title screen's too; the hash is
// of the lines the original's loops wrote, recorded before the zone was written as band tables.
static void ScrollBlock_EmeraldHillsLinesAreTheOriginals(void) {
    uint64_t hash = 0xcbf29ce484222325ull;
    for (int f = 0; f < 500; f++) {
        scrpos_x.f.u = (int16_t)(f * 13 - 100);
        Deform_EHZ();
        for (int l = 0; l < SCREEN_HEIGHT; l++)
            for (int k = 0; k < 2; k++) {
                hash = (hash ^ (uint8_t)hscroll_buffer[l][k]) * 0x100000001b3ull;
                hash = (hash ^ (uint8_t)((uint16_t)hscroll_buffer[l][k] >> 8)) * 0x100000001b3ull;
            }
    }
    scrpos_x.f.u = 0x1BC0;
    for (int f = 0; f < 40; f++) { // (the title's camera runs on by 8 up to $1C00)
        Deform_TitleScreen();
        for (int l = 0; l < SCREEN_HEIGHT; l++)
            for (int k = 0; k < 2; k++) {
                hash = (hash ^ (uint8_t)hscroll_buffer[l][k]) * 0x100000001b3ull;
                hash = (hash ^ (uint8_t)((uint16_t)hscroll_buffer[l][k] >> 8)) * 0x100000001b3ull;
            }
    }
    CHECK(hash == 0x356e9d00d3e1008bull);
}

// ---- the zones' backgrounds through the scrolling engine ----
// What a zone's background plane shows (the plane's tile under every 8th pixel of every line, through the scroll lines) over a run of 1200 frames of the camera going right and down, stopping, going
// left and up, then small moves both ways; the hash is of what the flag-driven drawing showed, recorded before the zones moved to the engine's band layers.

void LevelScroll_RegisterZones(void);
void LoadTilesAsYouMove(void);
extern uint8_t bg1_xblock, bg2_xblock, bg3_xblock, bg1_yblock, bg2_yblock, bg3_yblock;

static uint64_t ZoneBackgroundHash(int zone, int act) {
    uint64_t hash = 0xcbf29ce484222325ull;
    level_id = LEVEL_ID(zone, act);
    last_lamp = 0;
    camera_split = false;
    LevelScroll_RegisterZones();
    LevelDataLoad();
    Plane_Clear(&screen1p.plane_a);
    Plane_Clear(&screen1p.plane_b);
    int16_t x = StartLocArray[zone][act][0], y = StartLocArray[zone][act][1];
    x = (int16_t)(x < 160 ? 0 : x - 160);
    y = (int16_t)(y < 96 ? 0 : y - 96);
    scrpos_x.v = (int32_t)x << 16;
    scrpos_y.v = (int32_t)y << 16;
    bg_scrpos_x.v = bg_scrpos_y.v = bg2_scrpos_x.v = bg2_scrpos_y.v = bg3_scrpos_x.v = bg3_scrpos_y.v = 0;
    BgScrollSpeed(x, y);
    bg1_xblock = bg2_xblock = bg3_xblock = 0;
    bg1_yblock = bg2_yblock = bg3_yblock = 0;
    bg1_scroll_flags = bg2_scroll_flags = bg3_scroll_flags = 0;
    scrshift_x = scrshift_y = 0;
    vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;
    LoadTilesFromStart();
    for (int frame = 0; frame < 1200; frame++) {
        int sx, sy;
        if (frame < 400) { sx = (frame / 25) % 9; sy = (frame / 20) % 7; }
        else if (frame < 450) { sx = 0; sy = 0; }
        else if (frame < 900) { sx = -((frame / 25) % 9); sy = -((frame / 20) % 7); }
        else { sx = (frame % 7) - 3; sy = (frame % 11) - 5; }
        if (scrpos_x.f.u + sx < 0) sx = 0;
        if (scrpos_y.f.u + sy < 0) sy = 0;
        scrshift_x = (int16_t)(sx << 8);
        scrshift_y = (int16_t)(sy << 8);
        scrpos_x.v += (int32_t)sx << 16;
        scrpos_y.v += (int32_t)sy << 16;
        bg1_scroll_flags = bg2_scroll_flags = bg3_scroll_flags = 0;
        ScrollEngine_Events(zone, act);
        bg_scrpos_x_dup.v = bg_scrpos_x.v; bg_scrpos_y_dup.v = bg_scrpos_y.v;
        bg2_scrpos_x_dup.v = bg2_scrpos_x.v; bg2_scrpos_y_dup.v = bg2_scrpos_y.v;
        bg3_scrpos_x_dup.v = bg3_scrpos_x.v; bg3_scrpos_y_dup.v = bg3_scrpos_y.v;
        bg1_scroll_flags_dup = bg1_scroll_flags; bg2_scroll_flags_dup = bg2_scroll_flags; bg3_scroll_flags_dup = bg3_scroll_flags;
        LoadTilesAsYouMove();
        int vsy = vid_bg_scrpos_y_dup;
        for (int l = 0; l < SCREEN_HEIGHT; l++) {
            int row = ((l + vsy) >> 3) & (PLANE_HEIGHT - 1);
            int x0 = -hscroll_buffer[l][1];
            for (int px = 0; px < SCREEN_WIDTH; px += 8) {
                int col = ((x0 + px) >> 3) & (PLANE_WIDTH - 1);
                uint16_t w = Plane_Word(&screen1p.plane_b, (size_t)(row * PLANE_WIDTH + col) * 2);
                hash = (hash ^ (w & 0xFF)) * 0x100000001b3ull;
                hash = (hash ^ (w >> 8)) * 0x100000001b3ull;
            }
        }
    }
    return hash;
}

typedef struct { int zone, act; uint64_t hash; } ZoneGolden;
static const ZoneGolden zone_goldens[] = {
    { ZoneId_EHZ, 0, 0xca7815ae48c085a9ull },
    { ZoneId_CPZ, 0, 0xd5e4d0b49d087472ull },
    { ZoneId_HPZ, 0, 0xb7e71a2611871156ull },
    { ZoneId_HTZ, 0, 0x5ae5d58317443125ull },
    { ZoneId_WZ, 0, 0xe01b70f32a410a88ull },
    { ZoneId_MTZ, 0, 0xdcff82b672d2ac03ull },
    { ZoneId_OOZ, 0, 0xaa21dfdd6e90753ull },
    { ZoneId_MCZ, 0, 0x305ca2de88d0a7a0ull },
    { ZoneId_MCZ, 1, 0xfbc11d93656a3fbbull },
    { ZoneId_CNZ, 0, 0xa8858b0e01c44f30ull },
    { ZoneId_SSZ, 0, 0xd8755c5673d1e8f7ull },
    { ZoneId_ARZ, 0, 0xda475adf275582baull },
    { ZoneId_ARZ, 1, 0x22919f96ba252d92ull },
};

static void ScrollZones_BackgroundsShowWhatTheOriginalsDid(void) {
    for (size_t i = 0; i < sizeof(zone_goldens) / sizeof(zone_goldens[0]); i++) {
        uint64_t h = ZoneBackgroundHash(zone_goldens[i].zone, zone_goldens[i].act);
        CHECK(h == zone_goldens[i].hash);
    }
}

void RegisterScrollBlockTests(void) {
    RUN_TEST(ScrollZones_BackgroundsShowWhatTheOriginalsDid);
    RUN_TEST(ScrollBlock_EmeraldHillsLinesAreTheOriginals);
    RUN_TEST(ScrollBlock_RightAcrossALineRaisesTheRightFlag);
    RUN_TEST(ScrollBlock_LeftAcrossALineRaisesTheLeftFlag);
    RUN_TEST(ScrollBlock_AMoveInsideABlockRaisesNothing);
    RUN_TEST(ScrollBlock_NoMoveRaisesNothing);
    RUN_TEST(ScrollBlock_SingleStepsCrossTheLineOnce);
    RUN_TEST(ScrollBlock_FractionsAccumulate);
    RUN_TEST(ScrollBlock_FlagsAccumulate);
    RUN_TEST(ScrollBlock_TheFlagBitIsTheCallers);
    RUN_TEST(ScrollBlock_TheThreeBlocksAreIndependent);
    RUN_TEST(ScrollBlock_UpdateRaisesTheBitForTheDirectionMoved);
    RUN_TEST(ScrollBlock_UpdateInsideABlockRaisesNothing);
    RUN_TEST(ScrollBlock_BothTrackersAgreeAcrossASweep);
    RUN_TEST(ScrollBlock_XYUsesTheFirstFlagsForBothAxes);
    RUN_TEST(ScrollBlock_YUsesTheSecondRowOfFlags);
}
