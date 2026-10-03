#include "test.h"

#include <string.h>

#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Demo.h"
#include "Object.h"
#include "Object/DebugList.h"
#include "Object/SpecialResult.h"
#include "Object/SpecialSonic.h"
#include "PLC.h"
#include "SpecialStage.h"

// Special stage: Sonic (object 09) against the blocks of the layout, the touched-block animations, and the results
// card (objects 7E/7F). From s1disasm's "_incObj/09 Sonic in Special Stage.asm",
// "_inc/Special Stage Loading & Drawing.asm" and "_incObj/7E, 7F Special Stage Results and Chaos Emeralds.asm".

void SS_AniItems(void);

#define ROW(r) ((r) * SS_DIM)

// Sonic's position that puts the centre of cell (r, c) under the non-solid item check (it looks 80px above and
// 32px left of Sonic).
static void PlaceOnCell(int r, int c) {
    player->pos.l.x.v = (int32_t)(c * SS_BLOCKSIZE - 32 + 12) << 16;
    player->pos.l.y.v = (int32_t)(r * SS_BLOCKSIZE - 80 + 12) << 16;
}

static void Reset(void) {
    memset(ss_layout, 0, sizeof(ss_layout));
    SS_ClearAnimations();
    memset(player, 0, sizeof(Object));
    player->type = ObjId_SpecialSonic;
    ss_angle.v = 0;
    ss_rotate = SS_ROTATESPEED;
    emeralds = 0;
    memset(emerald_list, 0, sizeof(emerald_list));
    rings = 0;
    life_num = 0;
    lives = 3;
    continues = 0;
    gamemode = GameMode_Special;
    jpad1_hold2 = jpad1_press2 = 0;
    // Sonic stands on nothing in an empty stage: keep him still by overriding the frame's gravity afterwards.
}

// One frame of Sonic, then put him back (an empty stage would otherwise let him fall away from the cell).
static void Frame(int r, int c) {
    PlaceOnCell(r, c);
    player->routine = player->routine ? player->routine : 0;
    Obj_SpecialSonic(player);
    SS_AniItems();
}

static void SSonic_RingSparklesAndCounts(void) {
    Reset();
    ss_layout[ROW(40) + 40] = SSB_Ring;
    Frame(40, 40);
    CHECK_EQ(rings, 1);
    CHECK_EQ(ss_layout[ROW(40) + 40], SSB_Ring_Ani1);
    for (int i = 0; i < 30; i++)
        SS_AniItems();
    CHECK_EQ(ss_layout[ROW(40) + 40], 0); // the ring is gone once the sparkle ends
}

static void SSonic_FiftyRingsGiveOneContinue(void) {
    Reset();
    rings = 49;
    ss_layout[ROW(40) + 40] = SSB_Ring;
    Frame(40, 40);
    CHECK_EQ(continues, 1);
    ss_layout[ROW(41) + 40] = SSB_Ring;
    Frame(41, 40);
    CHECK_EQ(continues, 1); // only once
}

static void SSonic_EmeraldIsKeptAndEndsTheStage(void) {
    Reset();
    emeralds = 2;
    emerald_list[0] = 3;
    emerald_list[1] = 1;
    ss_layout[ROW(40) + 40] = SSB_Emerald1_Blue + 4; // the red one
    Frame(40, 40);
    CHECK_EQ(emeralds, 3);
    CHECK_EQ(emerald_list[2], 4);
    for (int i = 0; i < 40 && player->routine != 4; i++)
        SS_AniItems();
    CHECK_EQ(player->routine, 4); // the sparkle finished: Sonic starts the exit spin
}

static void SSonic_OneUpGivesALife(void) {
    Reset();
    ss_layout[ROW(40) + 40] = SSB_1Up;
    Frame(40, 40);
    CHECK_EQ(lives, 4);
}

static void SSonic_GoalStartsTheExitSpin(void) {
    Reset();
    ss_layout[ROW(40) + 40] = SSB_GOAL;
    // GOAL is solid: Sonic overlapping it sets the touched block.
    player->pos.l.x.v = (int32_t)(40 * SS_BLOCKSIZE - 20 + 4) << 16;
    player->pos.l.y.v = (int32_t)(40 * SS_BLOCKSIZE - 68 + 4) << 16;
    Obj_SpecialSonic(player);
    CHECK_EQ(player->routine, 4);

    for (int i = 0; i < 200 && gamemode == GameMode_Special; i++)
        Obj_SpecialSonic(player);
    CHECK_EQ(gamemode, GameMode_Level); // the stage spun up to 0x1800 and the level takes over
    CHECK_EQ(ss_rotate, 0x1800);
}

static void SSonic_UpAndDownBlocksChangeTheSpinSpeed(void) {
    Reset();
    int r = 40, c = 40;
    ss_layout[ROW(r) + c] = SSB_UP;
    player->pos.l.x.v = (int32_t)(c * SS_BLOCKSIZE - 20 + 4) << 16;
    player->pos.l.y.v = (int32_t)(r * SS_BLOCKSIZE - 68 + 4) << 16;
    Obj_SpecialSonic(player);
    CHECK_EQ(ss_rotate, SS_ROTATESPEED * 2);
    CHECK_EQ(ss_layout[ROW(r) + c], SSB_DOWN); // it became a DOWN block

    // Touching it again straight away does nothing (timeout)...
    ss_layout[ROW(r) + c] = SSB_UP;
    Obj_SpecialSonic(player);
    CHECK_EQ(ss_rotate, SS_ROTATESPEED * 2);
    // ...and a block at full speed doesn't double again: DOWN halves it back.
    Reset();
    ss_rotate = SS_ROTATESPEED * 2;
    ss_layout[ROW(r) + c] = SSB_DOWN;
    player->pos.l.x.v = (int32_t)(c * SS_BLOCKSIZE - 20 + 4) << 16;
    player->pos.l.y.v = (int32_t)(r * SS_BLOCKSIZE - 68 + 4) << 16;
    Obj_SpecialSonic(player);
    CHECK_EQ(ss_rotate, SS_ROTATESPEED);
    CHECK_EQ(ss_layout[ROW(r) + c], SSB_UP);
}

static void SSonic_RBlockReversesTheStage(void) {
    Reset();
    int r = 40, c = 40;
    ss_layout[ROW(r) + c] = SSB_R;
    player->pos.l.x.v = (int32_t)(c * SS_BLOCKSIZE - 20 + 4) << 16;
    player->pos.l.y.v = (int32_t)(r * SS_BLOCKSIZE - 68 + 4) << 16;
    Obj_SpecialSonic(player);
    CHECK_EQ((int16_t)ss_rotate, -SS_ROTATESPEED);
}

static void SSonic_BumperThrowsSonicAway(void) {
    Reset();
    int r = 40, c = 40;
    ss_layout[ROW(r) + c] = SSB_Bumper;
    player->pos.l.x.v = (int32_t)(c * SS_BLOCKSIZE - 20 + 4) << 16;
    player->pos.l.y.v = (int32_t)(r * SS_BLOCKSIZE - 68 + 4) << 16;
    Obj_SpecialSonic(player);
    CHECK(player->xsp != 0 || player->ysp != 0);
    CHECK(player->status.p.f.in_air);
    CHECK(SS_FindFreeAnimationSlot() != NULL);
    for (int i = 0; i < 60; i++)
        SS_AniItems();
    CHECK_EQ(ss_layout[ROW(r) + c], SSB_Bumper); // back to the idle bumper
}

static void SSonic_GlassWeakensThenBreaks(void) {
    Reset();
    int r = 40, c = 40;
    ss_layout[ROW(r) + c] = SSB_Glass1_Blue;
    for (int hit = 0; hit < 4; hit++) {
        player->pos.l.x.v = (int32_t)(c * SS_BLOCKSIZE - 20 + 4) << 16;
        player->pos.l.y.v = (int32_t)(r * SS_BLOCKSIZE - 68 + 4) << 16;
        Obj_SpecialSonic(player);
        for (int i = 0; i < 40; i++)
            SS_AniItems();
        if (hit < 3)
            CHECK_EQ(ss_layout[ROW(r) + c], SSB_Glass1_Blue + hit + 1); // the next, weaker colour
    }
    CHECK_EQ(ss_layout[ROW(r) + c], 0); // pink glass breaks away
}

static void SSonic_GhostBlocksTurnSolidAfterTheTrigger(void) {
    Reset();
    ss_layout[ROW(20) + 20] = SSB_Ghost;
    ss_layout[ROW(30) + 30] = SSB_Ghost;
    ss_layout[ROW(40) + 40] = SSB_InvGhostTrigger;
    Frame(20, 20); // pass a ghost block
    Frame(10, 10); // an empty cell: nothing yet... but this clears the state (the trigger must follow directly)
    CHECK_EQ(ss_layout[ROW(30) + 30], SSB_Ghost);

    Frame(20, 20);
    Frame(40, 40); // ghost block, then the invisible trigger
    Frame(10, 10); // then out into open space: every ghost block turns solid
    CHECK_EQ(ss_layout[ROW(20) + 20], SSB_RedWhite);
    CHECK_EQ(ss_layout[ROW(30) + 30], SSB_RedWhite);
}

// ---------------------------------------------------------------------------

static void SSResult_TalliesTheRingBonusThenSignalsExit(void) {
    memset(objects, 0, sizeof(objects));
    plc_buffer[0].art = NULL;
    rings = 7;
    ring_bonus = rings * 10;
    emeralds = 3;
    emerald_list[0] = 0; emerald_list[1] = 1; emerald_list[2] = 2;
    score = 0;
    restart = false;
    objects[SSR_CARD_SLOT].type = ObjId_SSResult;

    for (int f = 0; f < 3000 && !restart; f++) {
        for (int i = 0; i < 64; i++) {
            Object *o = &objects[i];
            if (o->type == ObjId_SSResult)
                Obj_SpecialResult(o);
            else if (o->type == ObjId_SSRChaos)
                Obj_SpecialResultEmerald(o);
        }
        vbla_count++;
    }
    CHECK(restart);
    CHECK_EQ(ring_bonus, 0);
    CHECK_EQ(score, 70); // 7 rings x 100 points (the score counts in tens)
    int emerald_objects = 0;
    for (int i = 0; i < 64; i++)
        if (objects[i].type == ObjId_SSRChaos)
            emerald_objects++;
    CHECK_EQ(emerald_objects, 3);
}

void Obj_GotThroughCard(Object *obj);

// Jumping into a giant ring (Obj_RingFlash sets big_ring) makes the end-of-act card lead to the special stage,
// with the next level already picked for afterwards; without it the next level just loads.
static void GiantRing_EndCardLeadsToTheSpecialStage(void) {
    for (int ring = 0; ring < 2; ring++) {
        memset(objects, 0, sizeof(objects));
        plc_buffer[0].art = NULL;
        level_id = LEVEL_ID(ZoneId_GHZ, 0);
        time_bonus = ring_bonus = 0;
        big_ring = (uint8_t)ring;
        restart = false;
        gamemode = GameMode_Level;
        last_lamp = 5;
        objects[23].type = ObjId_GotThroughCard;
        for (int f = 0; f < 3000 && gamemode == GameMode_Level && !restart; f++)
            for (int i = 0; i < 40; i++)
                if (objects[i].type == ObjId_GotThroughCard)
                    Obj_GotThroughCard(&objects[i]);
        CHECK_EQ(level_id, LEVEL_ID(ZoneId_GHZ, 1)); // GHZ2 comes next either way
        CHECK_EQ(last_lamp, 0);
        if (ring) {
            CHECK_EQ(gamemode, GameMode_Special);
        } else {
            CHECK_EQ(gamemode, GameMode_Level);
            CHECK(restart);
        }
    }
}

void PCycle_SS(void);

// The palette cycle (and with it the background mode) runs from a countdown that has to go below zero: it was an
// unsigned counter, so it never fired and the stage had no animated background at all.
static void SSBackground_CycleRunsOnTheOriginalSchedule(void) {
    palss_num = 0;
    palss_time = 0;
    static const struct { int call; uint16_t anim; } expected[] = {
        { 0, 0 },      // the grid, for 10 steps of 4 frames
        { 40, 8 },     // fish turn into birds (8 frames each)
        { 48, 0xA },
        { 56, 0xC },   // birds and clouds, for 512 frames twice
        { 56 + 512, 0xC },
        { 56 + 1024, 0xA },
    };
    int next = 0;
    uint16_t last = 0xFFFF;
    for (int call = 0; call <= 56 + 1024 + 4; call++) {
        PCycle_SS();
        if (ss_bg_anim != last) {
            last = ss_bg_anim;
            if (next < 6 && expected[next].call == call)
                CHECK_EQ(ss_bg_anim, expected[next].anim);
        }
    }
    CHECK_EQ(ss_bg_anim, 0xA);
    CHECK_EQ(palss_num, 15); // entries 0-14 have come up
}

// The stage number moves on every time a stage is entered, whether or not its emerald was won, and stages whose
// emerald you already have are skipped (Sonic 1 and Sonic 3 behaviour; only Sonic 2 forces a retry).
static void SSLoad_AdvancesEveryAttemptAndSkipsCollectedEmeralds(void) {
    emeralds = 0;
    last_special = 0;
    SS_Load(); // stage 1 (index 0), no emerald won
    CHECK_EQ(last_special, 1);
    SS_Load(); // the next attempt moves on to stage 2 anyway
    CHECK_EQ(last_special, 2);

    emeralds = 2; // stages 1 and 2 are won already
    emerald_list[0] = 0;
    emerald_list[1] = 1;
    last_special = 0;
    SS_Load();
    CHECK_EQ(last_special, 3); // it skipped to stage 3 (index 2)
    last_special = 5;
    SS_Load(); // the last stage isn't won: loaded, and the counter wraps round
    CHECK_EQ(last_special, 0);
    SS_Load(); // ...then the won ones are skipped again
    CHECK_EQ(last_special, 3);

    emeralds = 6; // all won: they just cycle
    last_special = 0;
    SS_Load();
    SS_Load();
    CHECK_EQ(last_special, 2);
}

static void SSonic_DebugModeEntersPlacesAndExits(void) {
    Reset();
    debug_mode = true;
    jpad1_press1 = JPAD_B;
    Obj_SpecialSonic(player);
    jpad1_press1 = 0;
    CHECK_EQ(debug_use, 1);
    gamemode = GameMode_Special;
    Obj_SpecialSonic(player); // setup frame: rotation holds still
    CHECK_EQ(debug_use, 2);
    CHECK_EQ(ss_rotate, 0);

    int count;
    const DebugListEntry *list = DebugList_Get(&count);
    CHECK_EQ(count, 13); // ring, bumper, 11 animals
    CHECK_EQ(list[1].type, ObjId_Bumper);

    jpad1_press1 = JPAD_C; // place the selected ring
    Obj_SpecialSonic(player);
    jpad1_press1 = JPAD_B; // leave
    Obj_SpecialSonic(player);
    jpad1_press1 = 0;
    CHECK_EQ(debug_use, 0);
    CHECK_EQ(ss_rotate, 0x40);
    CHECK(player->status.p.f.in_air);
    debug_mode = false;
}

static void SSDemo_FeedsTheStageDemoInput(void) {
    Reset();
    demo = 1;
    btn_pushtime1 = 0;
    btn_pushtime2 = intro_demo_ptr[7][1] - 1;
    for (int i = 0; i < intro_demo_ptr[7][1]; i++) {
        MoveSonicInDemo();
        CHECK_EQ(jpad1_hold1, intro_demo_ptr[7][0]); // the first record is held for its whole duration
    }
    MoveSonicInDemo();
    CHECK_EQ(jpad1_hold1, intro_demo_ptr[7][2]); // then the second record takes over
    demo = 0;
    jpad1_hold1 = jpad1_press1 = 0;
}

void RegisterSpecialStageTests(void) {
    RUN_TEST(SSonic_RingSparklesAndCounts);
    RUN_TEST(SSonic_FiftyRingsGiveOneContinue);
    RUN_TEST(SSonic_EmeraldIsKeptAndEndsTheStage);
    RUN_TEST(SSonic_OneUpGivesALife);
    RUN_TEST(SSonic_GoalStartsTheExitSpin);
    RUN_TEST(SSonic_UpAndDownBlocksChangeTheSpinSpeed);
    RUN_TEST(SSonic_RBlockReversesTheStage);
    RUN_TEST(SSonic_BumperThrowsSonicAway);
    RUN_TEST(SSonic_GlassWeakensThenBreaks);
    RUN_TEST(SSonic_GhostBlocksTurnSolidAfterTheTrigger);
    RUN_TEST(SSResult_TalliesTheRingBonusThenSignalsExit);
    RUN_TEST(GiantRing_EndCardLeadsToTheSpecialStage);
    RUN_TEST(SSBackground_CycleRunsOnTheOriginalSchedule);
    RUN_TEST(SSLoad_AdvancesEveryAttemptAndSkipsCollectedEmeralds);
    RUN_TEST(SSonic_DebugModeEntersPlacesAndExits);
    RUN_TEST(SSDemo_FeedsTheStageDemoInput);
}
