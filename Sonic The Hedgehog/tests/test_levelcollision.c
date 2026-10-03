#include "test.h"

#include "Level.h"
#include "LevelCollision.h"
#include "LevelDraw.h"
#include "LevelScroll.h"

// Regression test: load real GHZ act 1 data through the actual production
// loading path (LevelDataLoad/ColIndexLoad, not a standalone reimplementation)
// and confirm a known-solid ground cell is found solid via FindNearestTile.
// This exists because a standalone re-derivation of the same addressing
// formula (done by hand while debugging "Sonic falls through everything")
// matched the real source exactly, yet the compiled game still fell through
// -- meaning something in the real, linked path diverges from what a
// from-scratch reimplementation would predict, and this test is the only
// way to catch that gap.
static void GHZ1_KnownGroundCellIsSolid(void) {
    level_id = LEVEL_ID(ZoneId_GHZ, 0);
    LevelDataLoad();
    ColIndexLoad();

    // Layout row 6, column 4 resolves to chunk 9 in GHZ's real Map128 data
    // (raw layout byte indexes the table directly, no -1 shift -- see
    // FindNearestTile); within that chunk, cell (tx=2, ty=6) is word
    // 0x5010, which has a path-1 top-solid bit set. World position:
    // x = col*128 + tx*16 = 544, y = row*128 + ty*16 = 864.
    const uint8_t *tile = FindNearestTile(NULL, 544, 864);
    uint16_t word = (uint16_t)((tile[0] << 8) | tile[1]);

    CHECK(word == 0x5010);
    CHECK((word & META_SOLID_TOP_1) != 0);
    CHECK((word & META_TILE) != 0);
}

// Diagnostic probe (2026-08): checks whether solid ground actually exists at
// GHZ act 1's real spawn point (StartLocArray x=0x50, y=0x3B0), since a
// "Sonic falls through everything from the start" report needs to rule out
// the specific spawn location before suspecting the general addressing path
// (which GHZ1_KnownGroundCellIsSolid above already confirms works for at
// least one location).
static void GHZ1_SpawnPointHasFloorNearby(void) {
    level_id = LEVEL_ID(ZoneId_GHZ, 0);
    LevelDataLoad();
    ColIndexLoad();

    int16_t spawn_x = 0x50, spawn_y = 0x3B0;
    const uint8_t *tile = FindNearestTile(NULL, spawn_x, spawn_y);
    uint16_t word = (uint16_t)((tile[0] << 8) | tile[1]);
    printf("\n    [diag] spawn tile word=0x%04X tile_id=%u solid_top1=%d solid_lrb1=%d\n",
           word, word & META_TILE, (word & META_SOLID_TOP_1) != 0, (word & META_SOLID_LRB_1) != 0);

    // Probe a vertical strip below spawn to find the nearest solid cell.
    for (int16_t dy = 0; dy <= 128; dy += 16) {
        const uint8_t *t = FindNearestTile(NULL, spawn_x, spawn_y + dy);
        uint16_t w = (uint16_t)((t[0] << 8) | t[1]);
        if (w & META_SOLID_TOP_1) {
            printf("    [diag] nearest solid-top cell below spawn: dy=%d word=0x%04X\n", dy, w);
            break;
        }
        if (dy == 128)
            printf("    [diag] NO solid-top cell found within 128px below spawn\n");
    }
}

// Diagnostic probe (2026-08): dumps the exact chunk id and resolved tile id
// GetBlockData_2 (the real, linked drawing-side lookup -- not FindNearestTile,
// which is collision-side) returns across a full visible-screen grid, for
// both FG and BG planes, at real level-start scroll position. This is to
// directly answer "how is the engine reading the layout" against the actual
// compiled code, since eyeballing screenshots hasn't pinned down whether the
// remaining "layout looks wrong" report is a real addressing bug or just
// legitimately repetitive level content.
static void GHZ1_DumpVisibleScreenGrid(void) {
    level_id = LEVEL_ID(ZoneId_GHZ, 0);
    LevelDataLoad();
    ColIndexLoad();
    LevelSizeLoad();

    printf("\n    [diag] scrpos_x=%d scrpos_y=%d (real level-start camera)\n",
           scrpos_x.f.u, scrpos_y.f.u);

    printf("    [diag] FG chunk ids across visible screen (block-granularity, 16px steps):\n");
    for (int by = 0; by < 14; by++) {
        printf("    ");
        for (int bx = 0; bx < 20; bx++) {
            const uint8_t *meta = NULL, *block = NULL;
            GetBlockData(&meta, &block, scrpos_x.f.u, scrpos_y.f.u, bx * 16, by * 16, LEVEL_LAYOUT_FG(0));
            size_t tile = (size_t)((meta[0] << 8) | meta[1]) & META_TILE;
            printf("%3zx ", tile);
        }
        printf("\n");
    }
}

void RegisterLevelCollisionTests(void) {
    RUN_TEST(GHZ1_KnownGroundCellIsSolid);
    RUN_TEST(GHZ1_SpawnPointHasFloorNearby);
    RUN_TEST(GHZ1_DumpVisibleScreenGrid);
}
