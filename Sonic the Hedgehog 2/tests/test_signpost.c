#include "test.h"

#include "Level.h"
#include "PLC.h"

// The prototype loads the signpost's art when the camera reaches the end of the level, in every act but Emerald Hill's second (the boss's: End_Level_Art_Load). Nick Arcade skipped every second act, which
// kept Chemical Plant's and Neo Green Hill's second acts from ever loading their sign.

static bool SignpostQueued(uint16_t level) {
    level_id = level;
    debug_use = 0;
    time_count = 1;
    limit_right2 = 0x2000;
    limit_left2 = 0;
    scrpos_x.f.u = (int16_t)(limit_right2 - 0x100 - SCREEN_WIDEADD2 + 0x10);
    ClearPLC();
    SignpostArtLoad();
    return plc_buffer[0].art != NULL;
}

static void Signpost_EveryActButEmeraldHillsSecondLoadsItsArt(void) {
    CHECK(SignpostQueued(LEVEL_ID(ZoneId_EHZ, 0)));
    CHECK(SignpostQueued(LEVEL_ID(ZoneId_CPZ, 0)));
    CHECK(SignpostQueued(LEVEL_ID(ZoneId_CPZ, 1)));
    CHECK(SignpostQueued(LEVEL_ID(ZoneId_ARZ, 1)));
    CHECK(SignpostQueued(LEVEL_ID(ZoneId_HTZ, 1)));
}

static void Signpost_EmeraldHillsBossActLoadsNothing(void) {
    CHECK(!SignpostQueued(LEVEL_ID(ZoneId_EHZ, 1)));
}

static void Signpost_WaitsForTheEndOfTheLevel(void) {
    level_id = LEVEL_ID(ZoneId_CPZ, 1);
    debug_use = 0;
    time_count = 1;
    limit_right2 = 0x2000;
    limit_left2 = 0;
    scrpos_x.f.u = 0x400; // far from the end
    ClearPLC();
    SignpostArtLoad();
    CHECK(plc_buffer[0].art == NULL);
}

void RegisterSignpostTests(void) {
    RUN_TEST(Signpost_EveryActButEmeraldHillsSecondLoadsItsArt);
    RUN_TEST(Signpost_EmeraldHillsBossActLoadsNothing);
    RUN_TEST(Signpost_WaitsForTheEndOfTheLevel);
}
