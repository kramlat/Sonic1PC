#include "test.h"

#include <string.h>

#include "Level.h"
#include "Palette.h"

// The prototype's water: which zones have it, how high it starts, and its underwater palettes. CPZ's and NGHZ's cover all four lines and the first is Sonic's (and Tails'): that row is loaded live (PalLoad3_Water),
// and only loading the whole table (the fade's reference) once left Sonic and Tails black underwater.

extern uint16_t wet_palette[4][16];
extern const uint8_t S2Palette_CPZWater[];
extern const uint8_t S2Palette_NGHZWater[];

static void Water_OnlyTheRightZonesAndActsHaveIt(void) {
    level_id = LEVEL_ID(ZoneId_EHZ, 0);
    CHECK(!Level_HasWater());
    level_id = LEVEL_ID(ZoneId_HPZ, 0);
    CHECK(Level_HasWater());
    level_id = LEVEL_ID(ZoneId_CPZ, 0);
    CHECK(!Level_HasWater()); // (Chemical Plant's first act has none)
    level_id = LEVEL_ID(ZoneId_CPZ, 1);
    CHECK(Level_HasWater());
    level_id = LEVEL_ID(ZoneId_ARZ, 0);
    CHECK(Level_HasWater());
    level_id = LEVEL_ID(ZoneId_ARZ, 1);
    CHECK(Level_HasWater());
}

static void Water_StartHeightsMatchTheWaterHeightTable(void) {
    level_id = LEVEL_ID(ZoneId_CPZ, 1);
    CHECK_EQ(Level_WaterStartHeight(), 0x710);
    level_id = LEVEL_ID(ZoneId_ARZ, 0);
    CHECK_EQ(Level_WaterStartHeight(), 0x410);
    level_id = LEVEL_ID(ZoneId_ARZ, 1);
    CHECK_EQ(Level_WaterStartHeight(), 0x510);
}

static void CheckSonicsRow(const uint8_t *file) {
    int nonblack = 0;
    for (int c = 0; c < 16; c++) {
        CHECK_EQ(wet_palette[0][c], (uint16_t)((file[c * 2] << 8) | file[c * 2 + 1]));
        nonblack += wet_palette[0][c] != 0;
    }
    CHECK(nonblack > 8); // (the row is Sonic's colours, not a black one)
}

static void Water_ChemicalPlantLoadsSonicsUnderwaterRow(void) {
    level_id = LEVEL_ID(ZoneId_CPZ, 1);
    memset(wet_palette, 0, sizeof(wet_palette));
    Level_LoadWaterPalettes(true);
    CheckSonicsRow(S2Palette_CPZWater);
}

static void Water_NeoGreenHillLoadsSonicsUnderwaterRow(void) {
    level_id = LEVEL_ID(ZoneId_ARZ, 0);
    memset(wet_palette, 0, sizeof(wet_palette));
    Level_LoadWaterPalettes(true);
    CheckSonicsRow(S2Palette_NGHZWater);
}

void RegisterWaterTests(void) {
    RUN_TEST(Water_OnlyTheRightZonesAndActsHaveIt);
    RUN_TEST(Water_StartHeightsMatchTheWaterHeightTable);
    RUN_TEST(Water_ChemicalPlantLoadsSonicsUnderwaterRow);
    RUN_TEST(Water_NeoGreenHillLoadsSonicsUnderwaterRow);
}
