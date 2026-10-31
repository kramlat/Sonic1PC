#include "test.h"

#include <string.h>

#include "Game.h"
#include "Level.h"
#include "Object.h"
#include "SstLoad.h"
#include "Object/Ending.h"
#include "Object/EndingSonic.h"
#include "SpecialStage.h"

// The ending sequence's objects (87-89) and the "TRY AGAIN" / "END" screens' (8B, 8C): the cutscene's state machine, the
// emeralds' circle, and which emeralds Eggman juggles. From s1disasm's "_incObj/87, 88, 89 Ending Sequence ...asm" and
// "_incObj/8B, 8C Try Again, End Eggman, End Emeralds.asm".

static void Reset(void) {
    Sst_Reset();
    emeralds = 0;
    memset(emerald_list, 0, sizeof(emerald_list));
    restart = 0;
    gamemode = GameMode_Ending;
}

static int CountOf(uint8_t id) {
    int n = 0;
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code != NULL && sst[i].code == sst_object_codes[id])
            n++;
    return n;
}

// The n-th object of the id on the status table (in slot order)
static Sst *Nth(uint8_t id, int n) {
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code != NULL && sst[i].code == sst_object_codes[id] && n-- == 0)
            return &sst[i];
    return NULL;
}

static void RunSonic(int frames) {
    for (int i = 0; i < frames; i++)
        Obj_EndSonic(player);
}

static void Ending_BadEndingLeapsAndShowsTheLogo(void) {
    Reset();
    emeralds = 3;
    player->code = sst_object_codes[ObjId_87];
    RunSonic(1);
    CHECK_EQ(player->routine_secondary, 0x10); // skipped the emerald part
    RunSonic((3 * 60) + 36);             // the wait before the leap
    CHECK_EQ(CountOf(ObjId_89), 1);
}

static void Ending_GoodEndingMakesSixSpinningEmeralds(void) {
    Reset();
    emeralds = 6;
    player->code = sst_object_codes[ObjId_87];
    RunSonic(1);
    CHECK_EQ(player->routine_secondary, 2);
    RunSonic(79);                         // looking at the emeralds for a bit under 1.5 seconds
    CHECK_EQ(CountOf(ObjId_88), 1);
    Sst *first = Nth(ObjId_88, 0);
    Obj_EndChaos(first); // Sonic's "hold" animation hasn't reached its last frame: nothing yet
    CHECK_EQ(CountOf(ObjId_88), 1);

    player->mapping_frame = 2;
    Obj_EndChaos(first);
    CHECK_EQ(CountOf(ObjId_88), 6);
    for (int i = 0; i < 6; i++)
        CHECK_EQ(Nth(ObjId_88, i)->mapping_frame, 1 + i); // frame 0 is the white flash
    // They widen to a radius of 0x20 and the sequence moves on to the white flash
    player->routine_secondary = 6;
    for (int i = 0; i < 0x100; i++)
        for (int e = 0; e < 6; e++)
            Obj_EndChaos(Nth(ObjId_88, e));
    CHECK_EQ(first->own.u16[10], 0x2000);
    Obj_EndSonic(player);
    CHECK_EQ(restart, 1);
}

static void Ending_LogoSlidesInThenGoesToTheCredits(void) {
    Reset();
    Sst *logo = Sst_Spawn(ObjId_89, 0, 0);
    for (int i = 0; i < 20; i++)
        Obj_EndSTH(logo);
    CHECK_EQ(logo->routine, 4); // arrived, waiting
    CHECK_EQ(gamemode, GameMode_Ending);
    for (int i = 0; i < (5 * 60) + 1; i++)
        Obj_EndSTH(logo);
    CHECK_EQ(gamemode, GameMode_Credits);
}

static void TryAgain_EggmanJugglesTheMissingEmeralds(void) {
    Reset();
    emeralds = 4; // has blue, yellow, pink and green... as emerald_list order 0-3
    for (int i = 0; i < 4; i++)
        emerald_list[i] = (uint8_t)i;
    Sst *eggman = Sst_Spawn(ObjId_8B, 0, 0);
    Obj_EndEggman(eggman);
    int credits = 0;
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code != NULL && sst[i].code == sst_object_codes[ObjId_Credits])
            credits++;
    CHECK_EQ(credits, 1); // the "TRY AGAIN" text
    CHECK_EQ(credits_num, 9);
    CHECK_EQ(CountOf(ObjId_8C), 1);

    Sst *chaos = Nth(ObjId_8C, 0);
    Obj_TryChaos(chaos);
    CHECK_EQ(CountOf(ObjId_8C), 2);              // 6 - 4 = two emeralds
    CHECK_EQ(Nth(ObjId_8C, 0)->mapping_frame, 5); // the first one Sonic hasn't got (index 4), plus the flash frame
    CHECK_EQ(Nth(ObjId_8C, 1)->mapping_frame, 6);
}

static void TryAgain_AllEmeraldsIsTheEndScreen(void) {
    Reset();
    emeralds = 6;
    Sst *eggman = Sst_Spawn(ObjId_8B, 0, 0);
    Obj_EndEggman(eggman);
    CHECK_EQ(CountOf(ObjId_Credits), 0);
    CHECK_EQ(CountOf(ObjId_8C), 0);
    CHECK_EQ(eggman->anim, 2); // the tantrum
}

static void Ending_WreckedEggmobileFallsAndBlowsUp(void) {
    Reset();
    Sst *ship = Sst_Spawn(ObjId_8D, 0, 0);
    int blast_frame = -1;
    bool smoked = false;
    for (int f = 0; f < 600 && ship->code != NULL; f++) {
        for (int i = 0; i < SST_COUNT; i++) // the effect's parts too
            if (sst[i].code != NULL && sst[i].code == sst_object_codes[ObjId_8D])
                Obj_EndEggmobile(&sst[i]);
        if (ship->routine == 4 && blast_frame < 0)
            blast_frame = f;
        for (int i = 0; i < SST_COUNT; i++)
            if (sst[i].code != NULL && sst[i].code == sst_object_codes[ObjId_8D] && sst[i].routine == 6)
                smoked = true;
    }
    CHECK(smoked);                         // it trails smoke
    CHECK(blast_frame > 70 && blast_frame < 300); // sinks for a while, then blows up
    CHECK(ship->code == NULL);             // and is gone
}

void RegisterEndingTests(void) {
    RUN_TEST(Ending_WreckedEggmobileFallsAndBlowsUp);
    RUN_TEST(Ending_BadEndingLeapsAndShowsTheLogo);
    RUN_TEST(Ending_GoodEndingMakesSixSpinningEmeralds);
    RUN_TEST(Ending_LogoSlidesInThenGoesToTheCredits);
    RUN_TEST(TryAgain_EggmanJugglesTheMissingEmeralds);
    RUN_TEST(TryAgain_AllEmeraldsIsTheEndScreen);
}
