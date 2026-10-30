#include "test.h"

#include "Level.h"
#include "LevelLayout.h"
#include "Kosinski.h"
#include "DebugPeek.h"
#include "Palette.h"
#include "Backend/VDP.h"
#include <string.h>
#include "Constants.h"

// The Simon Wai prototype's level tables (Emerald Hill's, Metropolis's and Hill Top's are the alpha's since it was imported: these are the zones' rows that are still the prototype's, and Hill Top's), as its disassembly has them (s2b.asm: StartLocations, LevelSize, levartptrs): the port's own tables must say the same, zone by zone.

typedef struct {
    int zone, act;
    int16_t x, y; // StartLocations
} Start;

static void LevelData_StartPositionsMatchThePrototype(void) {
    static const Start starts[] = {
        { ZoneId_EHZ, 0, 0x60, 0x28F }, { ZoneId_EHZ, 1, 0x40, 0x2AF }, { ZoneId_HTZ, 0, 0x60, 0x3EF }, { ZoneId_HTZ, 1, 0x60, 0x6AF }, // (Hill Top's are the alpha's)
        { ZoneId_CNZ, 0, 0x60, 0x2AC }, { ZoneId_CNZ, 1, 0x60, 0x58C }, { ZoneId_CPZ, 0, 0x30, 0x1EC }, { ZoneId_CPZ, 1, 0x30, 0x12C },
        { ZoneId_ARZ, 0, 0x50, 0x37C }, { ZoneId_ARZ, 1, 0x50, 0x37C }, { ZoneId_WZ, 0, 0x60, 0x1AC },
    };
    for (size_t i = 0; i < sizeof(starts) / sizeof(starts[0]); i++) {
        CHECK_EQ(StartLocArray[starts[i].zone][starts[i].act][0], starts[i].x);
        CHECK_EQ(StartLocArray[starts[i].zone][starts[i].act][1], starts[i].y);
    }
}

typedef struct {
    int zone, act;
    int16_t right, top, bottom; // the level's right edge, and the top and bottom of the camera's range
} Size;

static void LevelData_LevelSizesMatchThePrototype(void) {
    static const Size sizes[] = {
        { ZoneId_HTZ, 0, 0x2800, 0, 0x720 }, { ZoneId_HTZ, 1, 0x3280, 0, 0x720 }, // (Hill Top's second act is the alpha's) { ZoneId_CPZ, 0, 0x2780, 0, 0x720 }, { ZoneId_CPZ, 1, 0x2880, 0, 0x720 },
        { ZoneId_CNZ, 0, 0x3FFF, 0, 0x720 }, { ZoneId_CNZ, 1, 0x3FFF, 0, 0x720 }, { ZoneId_ARZ, 0, 0x28C0, 0x200, 0x3A0 }, { ZoneId_ARZ, 1, 0x26C0, 0x180, 0x5A0 },
        { ZoneId_WZ, 0, 0x3FFF, 0, 0x720 },
    };
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        const int16_t *s = LevelSizes(sizes[i].zone, sizes[i].act);
        CHECK_EQ(s[2] - SCREEN_WIDEADD2, sizes[i].right);
        CHECK_EQ(s[3], sizes[i].top);
        CHECK_EQ(s[4] - SCREEN_TALLADD, sizes[i].bottom);
    }
}

static void LevelData_ArtListsAreThePrototypes(void) {
    // levartptrs: each zone's first and second sprite art list (the zones not built yet share Hill Top's first)
    CHECK_EQ(level_header[ZoneId_EHZ].plc1, PlcId_SLZ);
    CHECK_EQ(level_header[ZoneId_WZ].plc1, 0); // (Wood_Sprites_1 is only the zone's own art)
    CHECK_EQ(level_header[ZoneId_WZ].plc2, PlcId_SLZ2);
    CHECK_EQ(level_header[ZoneId_MCZ].plc1, PlcId_DHZ); // (Dust Hill: Dust_Hill_Sprites_1 and 2: the crate, collapsing platform, pull switch and vine; the spikes, gate log and springs)
    CHECK_EQ(level_header[ZoneId_MCZ].plc2, PlcId_DHZ2);
    CHECK_EQ(plcs[PlcId_DHZ]->plcs, 6);
    CHECK_EQ(plcs[PlcId_DHZ2]->plcs, 6);
    CHECK_EQ(plcs[PlcId_SLZ2]->plcs, 3); // Wood_Sprites_2: spikes, diagonal and up springs
    CHECK_EQ(level_header[ZoneId_HTZ].plc1, PlcId_SBZ);
    CHECK_EQ(level_header[ZoneId_HTZ].plc2, PlcId_SBZ2);
    CHECK_EQ(level_header[ZoneId_CPZ].plc1, PlcId_MZ);
    CHECK_EQ(level_header[ZoneId_CPZ].plc2, PlcId_MZ2);
    CHECK_EQ(level_header[ZoneId_CNZ].plc1, PlcId_LZ);
    CHECK_EQ(level_header[ZoneId_CNZ].plc2, PlcId_LZ2);
    CHECK_EQ(level_header[ZoneId_ARZ].plc1, PlcId_GHZ);
    CHECK_EQ(level_header[ZoneId_ARZ].plc2, PlcId_GHZ2);
}

// The rings manager walks a layout as the camera goes, so each must be in order of X (the prototypes and the alpha keep theirs in the order they were made in and sort them in RAM; the importer sorts ours)
static void LevelData_RingLayoutsAreInOrderOfX(void) {
    int layouts = 0, rings = 0;
    for (int zone = 0; zone < ZoneId_Num; zone++) {
        for (int act = 0; act < 4; act++) {
            const uint8_t *r = level_ring[zone][act];
            if (r == NULL)
                continue;
            layouts++;
            int last = -1;
            for (; r[0] != 0xFF || r[1] != 0xFF; r += 4, rings++) {
                const int x = (r[0] << 8) | r[1];
                CHECK(x >= last);
                last = x;
            }
        }
    }
    CHECK(layouts > 10);
    CHECK(rings > 500);
}

// Sand Shower's palette cycle: four colours go round in colours 8 to 11 of the fourth line, a step every 8 ticks
void PaletteCycle(void);
extern int16_t pcyc_time;
static void LevelData_SandShowerCyclesFourColoursOfTheFourthLine(void) {
    static const uint16_t colours[4] = { 0x0048, 0x046A, 0x048C, 0x08CE };
    level_id = LEVEL_ID(ZoneId_SSZ, 0);
    restart = 0;
    pcyc_time = 0;
    PaletteCycle();
    int first = -1;
    for (int c = 0; c < 4; c++) {
        if (dry_palette[3][8] == colours[c])
            first = c;
    }
    CHECK(first >= 0);
    for (int i = 0; i < 4; i++)
        CHECK_EQ(dry_palette[3][8 + i], colours[(first + i) & 3]);
    for (int tick = 0; tick < 7; tick++) { // (nothing changes until the 8th tick)
        PaletteCycle();
        CHECK_EQ(dry_palette[3][8], colours[first]);
    }
    PaletteCycle(); // (the step)
    for (int i = 0; i < 4; i++)
        CHECK_EQ(dry_palette[3][8 + i], colours[(first + 1 + i) & 3]);
}

// Sand Shower's cactus tops (tile $2E8, 5 tiles, five frames 0, 5, 10, 5, 0 of 7 ticks each): each step shows the next drawing, then back
void S2_PrimeLevelArt(void);
void S2_AnimateLevelArt(void);
static void LevelData_SandShowerCactusTopsSwayThroughThreeDrawings(void) {
    level_id = LEVEL_ID(ZoneId_SSZ, 0);
    const uint8_t *tiles = (const uint8_t *)VDP_TileSpace() + 0x2E8 * 0x20;
    S2_PrimeLevelArt();
    // (the counters are not started at the load: tick until the picture changes, and the steps follow every 7 ticks)
    uint8_t first[160];
    memcpy(first, tiles, 160);
    int ticks = 0;
    while (ticks < 40) {
        S2_AnimateLevelArt();
        ticks++;
        if (memcmp(first, tiles, 160) != 0)
            break;
    }
    CHECK(ticks < 40);
    uint8_t shown[10][160];
    memcpy(shown[0], tiles, 160);
    for (int step = 1; step < 10; step++) {
        for (int tick = 0; tick < 7; tick++)
            S2_AnimateLevelArt();
        memcpy(shown[step], tiles, 160);
    }
    // (the script is 0, 5, 10, 5, 0 and round again: a round of five steps in which the first drawing is shown twice)
    for (int i = 0; i < 5; i++)
        CHECK(memcmp(shown[i], shown[i + 5], 160) == 0);
    int distinct = 0;
    for (int i = 0; i < 5; i++) {
        bool seen = false;
        for (int j = 0; j < i; j++)
            seen = seen || memcmp(shown[i], shown[j], 160) == 0;
        distinct += !seen;
    }
    CHECK_EQ(distinct, 3);
}

// Cyber City's lights: the last colour of the fourth line pulses slowly between $0004 and $000E
static void LevelData_CyberCityLightsPulseBetweenTheirTwoColours(void) {
    level_id = LEVEL_ID(ZoneId_GCZ, 0);
    restart = 0;
    uint16_t low = 0xFFFF, high = 0;
    uint16_t last = 0;
    int changes = 0;
    for (int tick = 0; tick < 400; tick++) {
        PaletteCycle();
        const uint16_t c = dry_palette[3][15];
        if (c < low)
            low = c;
        if (c > high)
            high = c;
        changes += c != last;
        last = c;
    }
    CHECK_EQ(low, 0x0004);
    CHECK_EQ(high, 0x000E);
    CHECK(changes > 10 && changes < 400); // (a slow pulse: it is not a change every tick)
}

// Hill Top's tiles and blocks are Emerald Hill's with its own written over them from the supplement's start (HTZ_SUPPLEMENT_TILE and _BLOCK in Sonic2LevelData.c, which the editor
// shows and can move to try out: a note to make and a number to change there): the data must agree with those numbers. Hill Top's are Emerald Hill's up to the start, and the first
// tile (or block) that differs is the start
static void LevelData_HillTopsSupplementStartsWhereTheSymbolsSay(void) {
    int tile_start = 0, block_start = 0;
    CHECK(Peek_LevelSupplement(ZoneId_HTZ, &tile_start, &block_start));
    int other_tile = 0, other_block = 0;
    CHECK(!Peek_LevelSupplement(ZoneId_EHZ, &other_tile, &other_block)); // (the base has none)
    static uint8_t base[0x20000], top[0x20000];
    for (int kind = 0; kind < 2; kind++) {
        const uint8_t *base_data = kind ? level_header[ZoneId_EHZ].map16 : level_header[ZoneId_EHZ].art;
        const uint8_t *top_data = kind ? level_header[ZoneId_HTZ].map16 : level_header[ZoneId_HTZ].art;
        const int unit = kind ? 8 : 32, start = kind ? block_start : tile_start;
        memset(base, 0, sizeof(base));
        memset(top, 0, sizeof(top));
        KosDec(base_data, base);
        KosDec(top_data, top);
        int first = -1;
        for (int k = 0; k < (int)sizeof(base) && first < 0; k++)
            if (base[k] != top[k])
                first = k / unit;
        CHECK_EQ(first, start);
    }
}

// A level's layout says how big the level is (the Sonic 3 & Knuckles format): Hill Top's first act is 83 chunks wide and 16 rows tall in the foreground, 60 by 16 in the background; the null level has no chunk at all
static void LevelData_TheLayoutHeaderSaysHowBigTheLevelIs(void) {
    level_id = LEVEL_ID(ZoneId_HTZ, 0);
    LoadLevelLayout();
    CHECK_EQ(level_layout_size.fg_width, 83);
    CHECK_EQ(level_layout_size.fg_rows, 16);
    CHECK_EQ(level_layout_size.bg_width, 60);
    CHECK_EQ(level_layout_size.bg_rows, 16);
    level_id = LEVEL_ID(ZoneId_HTZ, 2); // (the null level)
    LoadLevelLayout();
    CHECK_EQ(level_layout_size.fg_width, 0);
    CHECK_EQ(level_layout_size.fg_rows, 0);
    CHECK_EQ(level_layout[0], 0);
}

void RegisterLevelDataTests(void) {
    RUN_TEST(LevelData_TheLayoutHeaderSaysHowBigTheLevelIs);
    RUN_TEST(LevelData_HillTopsSupplementStartsWhereTheSymbolsSay);
    RUN_TEST(LevelData_StartPositionsMatchThePrototype);
    RUN_TEST(LevelData_CyberCityLightsPulseBetweenTheirTwoColours);
    RUN_TEST(LevelData_SandShowerCactusTopsSwayThroughThreeDrawings);
    RUN_TEST(LevelData_SandShowerCyclesFourColoursOfTheFourthLine);
    RUN_TEST(LevelData_LevelSizesMatchThePrototype);
    RUN_TEST(LevelData_ArtListsAreThePrototypes);
    RUN_TEST(LevelData_RingLayoutsAreInOrderOfX);
}
