#include "test.h"

#include "Camera.h"
#include "LevelScroll.h"

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

void RegisterScrollBlockTests(void) {
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
