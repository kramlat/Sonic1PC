#include "test.h"

#include "LevelDraw.h"
#include "Level.h"
#include "LevelScroll.h"

#include <string.h>

// Regression test for GetBlockData_2's chunk-column mask (2026-08): it used
// "& 0x3F" (64 columns), aliasing any layout column >= 64 back into the
// first 64 -- but the real layout rows are 128 bytes/columns wide (see
// LoadLayout's "0x80 - (width+1)" row padding), so wide levels would read
// the wrong chunk data once the camera scrolled past column 64. Fixed to
// "& 0x7F" (128 columns), matching the disassembly's Calc_VRAM_Pos/
// GetBlockData_2.
static void GetBlockData2_ReadsColumnsPast64(void) {
    // One row (row 0) of a layout, 128 bytes wide, all empty (chunk 0)
    // except column 100 (chunk 1) -- past the old 0x3F/64 mask, but well
    // within the real 0x7F/128 range.
    static uint8_t layout[128 * 8];
    memset(layout, 0, sizeof(layout));
    layout[100] = 1;

    // Place known tile data for chunk 1 at block position (tx=3, ty=2), and
    // pick x/y so the lookup lands on column 100, row 0, tx=3, ty=2.
    // Chunks are 128x128 px / 8x8 cells (real Sonic 2 size); raw layout byte
    // indexes the table directly, no -1 shift:
    // metap = level_map128 + (chunk << 7) + (ty << 4) + (tx << 1)
    //       = level_map128 + 0xA6  (for chunk=1, ty=2, tx=3)
    level_map128[0xA6] = 0x00;
    level_map128[0xA7] = 0x05; // tile id 5

    int16_t x = (100 << 7) | 0x30; // column 100, tx = 3
    int16_t y = 0x20;              // row 0, ty = 2

    const uint8_t *meta = NULL, *block = NULL;
    GetBlockData_2(&meta, &block, /*sy=*/0, x, y, layout);

    // Before the fix, column 100 aliased to (100 & 0x3F) == 36, which is
    // empty (chunk 0) in this layout, so *block would fall back to the
    // default level_map16 base instead of resolving tile 5.
    CHECK(block == level_map16 + (5 << 3));
}

// SBZ act 1 (REV01) background scrolling: Draw_SBZ only looks at the redraw flags the real routines set --
// block 1 at bits 2/3, block 2 at 4/5, block 3 at 6/7 (right-scroll = the upper bit of each pair) and the
// top/bottom row at bits 0/1. Passing the wrong bit (or BGScroll_Y's bits 4/5 for the vertical move) left new
// right-edge columns and new bottom rows undrawn or drawn from the wrong layer.
static void SBZ1_BackgroundRedrawFlags(void) {
    level_id = LEVEL_ID(ZoneId_SBZ, 0);
    bg_scrpos_x.v = bg2_scrpos_x.v = bg3_scrpos_x.v = bg_scrpos_y.v = 0;
    bg1_xblock = bg2_xblock = bg3_xblock = 0;
    bg1_yblock = 0x10; // makes the first move count as crossing a 16px boundary
    bg1_scroll_flags = bg2_scroll_flags = bg3_scroll_flags = 0;
    scrshift_x = 0x10; // moving right...
    scrshift_y = 0x10; // ...and down
    scrpos_x.v = 0;
    DeformLayers();
    CHECK_EQ(bg2_scroll_flags & 0xFF, 0x08 | 0x20 | 0x80 | 0x02);
    CHECK_EQ(bg1_scroll_flags & 0xFF, 0);
}

// SBZ act 1's background row map must be the real BG_ScrollBlockMap_SBZ (which BG layer each 16px row follows).
// A hand-tuned stand-in used to be here, which sent new columns to the wrong rows (missing smoke-puff blocks etc.).
static void SBZ_ScrollArrayMatchesDisassembly(void) {
    static const uint8_t real[34] = {
        0, 0, 0, 0, 0, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 4,
        4, 4, 4, 4, 4, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        2, 0,
    };
    for (int i = 0; i < 34; i++)
        CHECK_EQ(SBZ_ScrollArray[i], real[i]);
}

void RegisterLevelDrawTests(void) {
    RUN_TEST(GetBlockData2_ReadsColumnsPast64);
    RUN_TEST(SBZ_ScrollArrayMatchesDisassembly);
    RUN_TEST(SBZ1_BackgroundRedrawFlags);
}
