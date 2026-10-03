#include "test.h"

#include "PLC.h"
#include "Constants.h"

#include <stdbool.h>

// Regression tests for the two PLC.c bugs found while debugging the
// titlecard hang/crash (2026-08): the queue that GM_Level pushes for a GHZ
// level start (title card + GHZ level art + Main2) fills plc_buffer to
// *exactly* 16/16 entries, leaving zero slack for a NULL sentinel. That
// exposed two separate bugs:
//   1. The pop/shift in ProcessDPLC_Main didn't clear the vacated tail
//      slot, so once the queue drained down to it, the same stale entry
//      kept getting re-copied into slot 0 forever (infinite hang).
//   2. AddPLC's "find an empty slot" scan had no bound, so any call made
//      while the buffer was completely full walked off the end of the
//      16-slot array into adjacent memory (crash / corruption).

static void PLC_DrainsExactlyFullQueue(void) {
    ClearPLC();
    NewPLC(PlcId_TitleCard);
    AddPLC(PlcId_GHZ);
    AddPLC(PlcId_Main2);
    // TitleCard(1) + GHZ(10) + Main2(3) = 14 since PLC_GHZ dropped its
    // Art_GHZ1/Art_GHZ2 entries (now loaded separately via KosDec, not the
    // Nemesis-only PLC pipeline -- see LevelDataLoad). Top up to the real
    // 16-slot edge case with an unrelated small list.
    AddPLC(PlcId_LZAnimals);

    CHECK(plc_buffer[0].art != NULL);

    int iterations = 0;
    const int max_iterations = 2000; // real drain takes ~150-200 at 9 tiles/frame
    while (plc_buffer[0].art != NULL && iterations < max_iterations) {
        RunPLC();
        ProcessDPLC();
        iterations++;
    }

    CHECK(plc_buffer[0].art == NULL);
    CHECK(iterations < max_iterations);
}

static void PLC_AddPLCDoesNotOverflowWhenFull(void) {
    ClearPLC();
    NewPLC(PlcId_TitleCard);
    AddPLC(PlcId_GHZ);
    AddPLC(PlcId_Main2);
    AddPLC(PlcId_LZAnimals); // top up to 16, see PLC_DrainsExactlyFullQueue

    for (int i = 0; i < 16; i++)
        CHECK(plc_buffer[i].art != NULL);

    // Buffer is completely full; these must be safely dropped, not corrupt
    // adjacent memory (this is exactly the scenario ASan caught -- run this
    // suite under -DSANITIZE=ON to get that coverage).
    AddPLC(PlcId_Explode);
    AddPLC(PlcId_GHZAnimals);

    for (int i = 0; i < 16; i++)
        CHECK(plc_buffer[i].art != NULL);
}

// The P128 migration (d414074) dropped Art_Splash from PLC_LZ along with the
// level-art entries, but it is object art (waterfalls and splashes), not part
// of the Kosinski level art -- every waterfall drew level tiles instead.
extern const uint8_t Art_Splash[];

static void PLC_LZLoadsWaterfallSplashArt(void) {
    ClearPLC();
    NewPLC(PlcId_LZ);
    bool found = false;
    for (int i = 0; i < 16 && plc_buffer[i].art != NULL; i++)
        if (plc_buffer[i].art == Art_Splash && plc_buffer[i].off == ART_VRAM(ArtTile_LZ_Splash))
            found = true;
    CHECK(found);
    ClearPLC();
}

// SBZ's collapsing floor art (Nem_SbzFloor) is 4 tiles, loaded at $3F5 in SBZ1 and
// $3F9 in SBZ2. res/Art/SBZFloor used to be a copy of the 15-tile sliding-floor
// art, so the floors drew wrong graphics and, in SBZ2, the extra tiles spilled
// over the art after tile $3F9.
extern const uint8_t Art_SBZFloor[];
extern const uint8_t Art_SlideFloor[];
static void PLC_SBZCollapsingFloorArtIsFourTiles(void) {
    CHECK_EQ(((Art_SBZFloor[0] << 8) | Art_SBZFloor[1]) & 0x7FFF, 4);
    CHECK(((Art_SlideFloor[0] << 8) | Art_SlideFloor[1]) != ((Art_SBZFloor[0] << 8) | Art_SBZFloor[1]));
}

// SLZ loaded the bomb enemy art into the Orbinaut slot ($429), where the Orbinaut art
// overwrote it, but the object reads it from ArtTile_Bomb ($400).
extern const uint8_t Art_Bomb[];
static void PLC_SLZLoadsBombArtAtBombTile(void) {
    ClearPLC();
    NewPLC(PlcId_SLZ);
    bool found = false;
    for (int i = 0; i < 16 && plc_buffer[i].art != NULL; i++)
        if (plc_buffer[i].art == Art_Bomb && plc_buffer[i].off == ART_VRAM(ArtTile_Bomb))
            found = true;
    CHECK(found);
    ClearPLC();
}

void RegisterPLCTests(void) {
    RUN_TEST(PLC_DrainsExactlyFullQueue);
    RUN_TEST(PLC_AddPLCDoesNotOverflowWhenFull);
    RUN_TEST(PLC_LZLoadsWaterfallSplashArt);
    RUN_TEST(PLC_SBZCollapsingFloorArtIsFourTiles);
    RUN_TEST(PLC_SLZLoadsBombArtAtBombTile);
}
