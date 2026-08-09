#include "test.h"

#include "PLC.h"

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

void RegisterPLCTests(void) {
    RUN_TEST(PLC_DrainsExactlyFullQueue);
    RUN_TEST(PLC_AddPLCDoesNotOverflowWhenFull);
}
