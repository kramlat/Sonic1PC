#include "test.h"

#include "LevelDraw.h"
#include "Level.h"

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

void RegisterLevelDrawTests(void) {
    RUN_TEST(GetBlockData2_ReadsColumnsPast64);
}
