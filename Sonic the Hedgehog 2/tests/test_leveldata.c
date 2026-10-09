#include "test.h"

#include "Level.h"
#include "Constants.h"

// The Simon Wai prototype's level tables, as its disassembly has them (s2b.asm: StartLocations, LevelSize, levartptrs): the port's own tables must say the same, zone by zone.

typedef struct {
    int zone, act;
    int16_t x, y; // StartLocations
} Start;

static void LevelData_StartPositionsMatchThePrototype(void) {
    static const Start starts[] = {
        { ZoneId_EHZ, 0, 0x60, 0x28F }, { ZoneId_EHZ, 1, 0x40, 0x2AF }, { ZoneId_HTZ, 0, 0x40, 0x3AF }, { ZoneId_HTZ, 1, 0x60, 0x68F },
        { ZoneId_CNZ, 0, 0x60, 0x28F }, { ZoneId_CNZ, 1, 0x40, 0x2AF }, { ZoneId_CPZ, 0, 0x30, 0x1EC }, { ZoneId_CPZ, 1, 0x30, 0x12C },
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
        { ZoneId_HTZ, 0, 0x2800, 0, 0x720 }, { ZoneId_HTZ, 1, 0x2880, 0, 0x720 }, { ZoneId_CPZ, 0, 0x2780, 0, 0x720 }, { ZoneId_CPZ, 1, 0x2880, 0, 0x720 },
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
    CHECK_EQ(plcs[PlcId_DHZ]->plcs, 4);
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

void RegisterLevelDataTests(void) {
    RUN_TEST(LevelData_StartPositionsMatchThePrototype);
    RUN_TEST(LevelData_LevelSizesMatchThePrototype);
    RUN_TEST(LevelData_ArtListsAreThePrototypes);
}
