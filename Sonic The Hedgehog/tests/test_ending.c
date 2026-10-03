#include "test.h"

#include <string.h>

#include "Game.h"
#include "Level.h"
#include "Object.h"
#include "Object/Ending.h"
#include "SpecialStage.h"

// The ending sequence's objects (87-89) and the "TRY AGAIN" / "END" screens' (8B, 8C): the cutscene's state machine, the
// emeralds' circle, and which emeralds Eggman juggles. From s1disasm's "_incObj/87, 88, 89 Ending Sequence ...asm" and
// "_incObj/8B, 8C Try Again, End Eggman, End Emeralds.asm".

static void Reset(void) {
    memset(objects, 0, sizeof(objects));
    emeralds = 0;
    memset(emerald_list, 0, sizeof(emerald_list));
    restart = 0;
    gamemode = GameMode_Ending;
}

static void RunSonic(int frames) {
    for (int i = 0; i < frames; i++)
        Obj_EndSonic(player);
}

static void Ending_BadEndingLeapsAndShowsTheLogo(void) {
    Reset();
    emeralds = 3;
    player->type = ObjId_87;
    RunSonic(1);
    CHECK_EQ(player->routine_sec, 0x10); // skipped the emerald part
    RunSonic((3 * 60) + 36);             // the wait before the leap
    CHECK_EQ(objects[ENDING_SLOT_LOGO].type, ObjId_89);
}

static void Ending_GoodEndingMakesSixSpinningEmeralds(void) {
    Reset();
    emeralds = 6;
    player->type = ObjId_87;
    RunSonic(1);
    CHECK_EQ(player->routine_sec, 2);
    RunSonic(79);                         // looking at the emeralds for a bit under 1.5 seconds
    CHECK_EQ(objects[ENDING_SLOT_EMERALDS].type, ObjId_88);
    Obj_EndChaos(&objects[ENDING_SLOT_EMERALDS]); // Sonic's "hold" animation hasn't reached its last frame: nothing yet
    CHECK_EQ(objects[ENDING_SLOT_EMERALDS + 1].type, 0);

    player->frame = 2;
    Obj_EndChaos(&objects[ENDING_SLOT_EMERALDS]);
    for (int i = 0; i < 6; i++) {
        CHECK_EQ(objects[ENDING_SLOT_EMERALDS + i].type, ObjId_88);
        CHECK_EQ(objects[ENDING_SLOT_EMERALDS + i].frame, 1 + i); // frame 0 is the white flash
    }
    // They widen to a radius of 0x20 and the sequence moves on to the white flash
    player->routine_sec = 6;
    for (int i = 0; i < 0x100; i++)
        for (int e = 0; e < 6; e++)
            Obj_EndChaos(&objects[ENDING_SLOT_EMERALDS + e]);
    CHECK_EQ(objects[ENDING_SLOT_EMERALDS].scratch.u16[10], 0x2000);
    Obj_EndSonic(player);
    CHECK_EQ(restart, 1);
}

static void Ending_LogoSlidesInThenGoesToTheCredits(void) {
    Reset();
    Object *logo = &objects[ENDING_SLOT_LOGO];
    logo->type = ObjId_89;
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
    objects[CREDITS_SLOT_TEXT].type = ObjId_8B;
    Obj_EndEggman(&objects[CREDITS_SLOT_TEXT]);
    CHECK_EQ(objects[CREDITS_SLOT_TRYAGAIN].type, ObjId_Credits); // the "TRY AGAIN" text
    CHECK_EQ(credits_num, 9);
    CHECK_EQ(objects[CREDITS_SLOT_CHAOS].type, ObjId_8C);

    Object *chaos = &objects[CREDITS_SLOT_CHAOS];
    Obj_TryChaos(chaos);
    CHECK_EQ(chaos[0].type, ObjId_8C);
    CHECK_EQ(chaos[1].type, ObjId_8C);
    CHECK_EQ(chaos[2].type, 0);                  // 6 - 4 = two emeralds
    CHECK_EQ(chaos[0].frame, 5);                 // the first one Sonic hasn't got (index 4), plus the flash frame
    CHECK_EQ(chaos[1].frame, 6);
}

static void TryAgain_AllEmeraldsIsTheEndScreen(void) {
    Reset();
    emeralds = 6;
    objects[CREDITS_SLOT_TEXT].type = ObjId_8B;
    Obj_EndEggman(&objects[CREDITS_SLOT_TEXT]);
    CHECK_EQ(objects[CREDITS_SLOT_TRYAGAIN].type, 0);
    CHECK_EQ(objects[CREDITS_SLOT_CHAOS].type, 0);
    CHECK_EQ(objects[CREDITS_SLOT_TEXT].anim, 2); // the tantrum
}

static void Ending_WreckedEggmobileFallsAndBlowsUp(void) {
    Reset();
    Object *ship = &objects[ENDING_SLOT_EGGMOBILE];
    ship->type = ObjId_8D;
    int blast_frame = -1;
    bool smoked = false;
    for (int f = 0; f < 600 && ship->type != 0; f++) {
        for (int i = 0; i < 128; i++) // the effect's parts too (the level object slots)
            if (objects[i].type == ObjId_8D)
                Obj_EndEggmobile(&objects[i]);
        if (ship->routine == 4 && blast_frame < 0)
            blast_frame = f;
        for (int i = 32; i < 128; i++)
            if (objects[i].type == ObjId_8D && objects[i].routine == 6)
                smoked = true;
    }
    CHECK(smoked);                         // it trails smoke
    CHECK(blast_frame > 70 && blast_frame < 300); // sinks for a while, then blows up
    CHECK_EQ(ship->type, 0);               // and is gone
    int left = 0;
    for (int i = 0; i < 128; i++)
        left += objects[i].type == ObjId_8D;
    CHECK(left > 0 || blast_frame > 0);
}

void RegisterEndingTests(void) {
    RUN_TEST(Ending_WreckedEggmobileFallsAndBlowsUp);
    RUN_TEST(Ending_BadEndingLeapsAndShowsTheLogo);
    RUN_TEST(Ending_GoodEndingMakesSixSpinningEmeralds);
    RUN_TEST(Ending_LogoSlidesInThenGoesToTheCredits);
    RUN_TEST(TryAgain_EggmanJugglesTheMissingEmeralds);
    RUN_TEST(TryAgain_AllEmeraldsIsTheEndScreen);
}
