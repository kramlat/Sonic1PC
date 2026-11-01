#include "test.h"

#include <string.h>

#include "Level.h"
#include "Object.h"
#include "Object/Crocobot.h"
#include "Object/DHZObjects.h"
#include "Object/NGHZBadniks.h"
#include "Object/NGHZBoss.h"
#include "Object/BossCommon.h"
#include "Object/NGHZObjects.h"
#include "Object/Sonic.h"
#include "SstLoad.h"

// Neo Green Hill's arrow shooter (22) and leaves (2C), as the prototype's routines have them.

// (the slots below $20 are taken, so that the objects a test makes get the slots the old tests named)
static void SstTestFiller(Sst *self) { (void)self; }

static void Reset(void) {
    Sst_Reset();
    sst_load_everything_in_range = true;
    for (int i = 4; i < 0x20; i++)
        sst[i].code = SstTestFiller;
    static const SstObjectIndex index_ = {sst_object_codes, 0x100};
    SstLoadCamera camera = {0, 0, false, 0xFFF};
    SstLoad_Init(&index_, NULL, false, &camera);
    level_id = LEVEL_ID(ZoneId_ARZ, 0);
    scrpos_x.v = scrpos_y.v = 0;
    frame_count = 0;
    player->routine = 2;
    player->x_pos.f.u = 0x1000;
    player->y_pos.f.u = 0x1000;
}

static Sst *Spawn(uint8_t type, uint8_t subtype, int16_t x, int16_t y) {
    SstLoad_Seek((int16_t)(x - 0x100)); // (the loader keeps what is near the camera: the camera is where the object is)
    Sst *obj = Sst_Spawn(type, x, y);
    obj->subtype = subtype;
    return obj;
}

static int Count(uint8_t type, uint8_t routine) {
    int n = 0;
    for (int i = 1; i < 0x60; i++)
        n += sst[i].code == sst_object_codes[type] && sst[i].routine == routine;
    return n;
}

// The same for the objects of the status table
static Sst *SpawnSst(uint8_t type, uint8_t subtype, int16_t x, int16_t y) {
    SstLoad_Seek((int16_t)(x - 0x100)); // (the loader keeps what is near the camera: the camera is where the object is)
    Sst *obj = Sst_Spawn(type, x, y);
    obj->subtype = subtype;
    return obj;
}

static int CountSst(uint8_t type, uint8_t routine) {
    int n = 0;
    for (int i = 0; i < SST_COUNT; i++)
        n += sst[i].code != NULL && sst[i].code == sst_object_codes[type] && sst[i].routine == routine;
    return n;
}

static void NGHZObjects_TheShooterFiresWhenSonicHasBeenNearAndGoes(void) {
    Reset();
    Sst *obj = SpawnSst(0x22, 0, 100, 200);
    for (int i = 0; i < 100; i++)
        Obj_ArrowShooter(obj);
    CHECK_EQ(CountSst(0x22, 6) + CountSst(0x22, 8), 0); // (nobody near: no arrow)
    player->x_pos.f.u = 120; // within $40: it is ready
    for (int i = 0; i < 40; i++)
        Obj_ArrowShooter(obj);
    CHECK_EQ(CountSst(0x22, 6) + CountSst(0x22, 8), 0); // (and still holds its fire)
    player->x_pos.f.u = 0x1000; // he leaves: now it draws back and shoots
    int frames = 0;
    while (frames++ < 400 && CountSst(0x22, 6) + CountSst(0x22, 8) == 0)
        Obj_ArrowShooter(obj);
    CHECK(CountSst(0x22, 6) + CountSst(0x22, 8) >= 1);
}

static void NGHZObjects_AnArrowFliesTheWayTheShooterFaces(void) {
    Reset();
    Sst *obj = SpawnSst(0x22, 0, 100, 200);
    Obj_ArrowShooter(obj);
    Sst *arrow = Sst_Spawn(0x22, 100, 200);
    arrow->routine = 6;
    arrow->mappings = obj->mappings;
    Obj_ArrowShooter(arrow);
    CHECK_EQ(arrow->collision_flags, 0x9B); // (it hurts)
    CHECK_EQ(arrow->x_vel, 0x400);
    Sst *back = Sst_Spawn(0x22, 100, 200);
    back->routine = 6;
    back->status |= SST_STATUS_X_FLIP;
    back->mappings = obj->mappings;
    Obj_ArrowShooter(back);
    CHECK_EQ(back->x_vel, -0x400);
}

static void NGHZObjects_TheLeavesBoxHurtsNothingAndIsKeyedBySubtype(void) {
    Reset();
    static const uint8_t expected[3] = { 0xD6, 0xD4, 0xD5 }; // (the touch response's "special" kinds, the leaves' boxes)
    for (int sub = 0; sub < 3; sub++) {
        Sst *obj = SpawnSst(0x2C, (uint8_t)sub, 100, 200);
        Obj_NGHZLeaves(obj);
        CHECK_EQ(obj->collision_flags, expected[sub]);
        CHECK_EQ(obj->width_pixels, 0x80);
    }
}

static void NGHZObjects_FastSonicInTheBoxMakesFourLeaves(void) {
    Reset();
    Sst *obj = SpawnSst(0x2C, 0, 100, 200);
    Obj_NGHZLeaves(obj);
    player->x_pos.f.u = 100;
    player->y_pos.f.u = 200;
    player->x_vel = 0x400; // fast
    obj->collision_property = 1; // (the touch response: Sonic is in the box)
    frame_count = 0x10;     // (a frame that is a multiple of 16: Sonic's turn)
    Obj_NGHZLeaves(obj);
    CHECK_EQ(CountSst(0x2C, 4), 4);
}

static void NGHZObjects_SlowSonicMakesNoLeaves(void) {
    Reset();
    Sst *obj = SpawnSst(0x2C, 0, 100, 200);
    Obj_NGHZLeaves(obj);
    player->x_vel = 0x100;
    player->y_vel = 0x100;
    obj->collision_property = 1;
    frame_count = 0x10;
    Obj_NGHZLeaves(obj);
    CHECK_EQ(CountSst(0x2C, 4), 0);
}

// --- the alpha's boss and badniks ---

static void NGHZObjects_TheBossArenaLocksTheCameraAndLoadsTheBossOnce(void) {
    Reset();
    level_id = LEVEL_ID(ZoneId_ARZ, 1);
    dle_routine = 0;
    scrpos_x.f.u = 0x28A0 - SCREEN_WIDEADD2 - 1;
    NGHZBoss_Events();
    CHECK_EQ(dle_routine, 0);
    scrpos_x.f.u = 0x28A0 - SCREEN_WIDEADD2;
    NGHZBoss_Events();
    CHECK_EQ(dle_routine, 2);
    CHECK_EQ(boss_flag, 4);
    CHECK_EQ(limit_btm1, 0x400 - SCREEN_TALLADD);
    CHECK_EQ(Count(0x89, 0), 1);
    scrpos_x.f.u = 0x2A40 - SCREEN_WIDEADD2;
    NGHZBoss_Events();
    CHECK_EQ(dle_routine, 4);
    CHECK_EQ(limit_right2, 0x2A40 + SCREEN_WIDEADD2);
    ClearPLC();
    boss_flag = 0;
    dle_routine = 0;
}

static void NGHZObjects_TheBossWaitsForSonicInTheArenaThenDescendsWithItsLauncher(void) {
    Reset();
    level_id = LEVEL_ID(ZoneId_ARZ, 1);
    Sst *boss = Spawn(0x89, 0, 0, 0);
    limit_left2 = 0x2A40 - SCREEN_WIDEADD2;
    player->x_pos.f.u = 0x2A00; // (not yet in)
    Obj_NGHZBoss(boss);
    CHECK_EQ(boss->routine, 0);
    player->x_pos.f.u = 0x2AD8;
    Obj_NGHZBoss(boss);
    CHECK_EQ(boss->child_count, 3);
    CHECK_EQ(boss->collision_flags, 0x0F);
    CHECK_EQ(boss->x_pos.f.u, 0x2AD8);
    CHECK_EQ(boss->y_pos.f.u, 0x388);
    int launchers = 0;
    for (int i = 1; i < 0x60; i++)
        launchers += sst[i].code == sst_object_codes[0x89] && sst[i].child_count == 2;
    CHECK_EQ(launchers, 1);
}

static void NGHZObjects_TheWhispFollowsSonicWithinItsSpeedLimit(void) {
    Reset();
    Sst *whisp = Spawn(0x8C, 0, 400, 300);
    player->x_pos.f.u = 600;
    player->y_pos.f.u = 200;
    Obj_Whisp(whisp);
    CHECK_EQ(whisp->collision_flags, 0x0B);
    Obj_Whisp(whisp);
    CHECK_EQ(whisp->routine, 2); // (it waits until it is on screen)
    whisp->render_flags |= SST_RENDER_ON_SCREEN;
    Obj_Whisp(whisp);
    CHECK_EQ(whisp->routine, 4); // (it has set off)
    for (int i = 0; i < 0x30; i++)
        Obj_Whisp(whisp);
    CHECK(whisp->x_vel > 0 && whisp->x_vel <= 0x200); // (towards Sonic, on the right)
    CHECK(whisp->y_vel < 0 && whisp->y_vel >= -0x200); // (and up)
    CHECK(whisp->x_pos.f.u > 400);
}

static void NGHZObjects_TheGrounderBreaksItsWallAndWalksToSonic(void) {
    Reset();
    Sst *grounder = Spawn(0x8D, 0, 500, 300);
    player->x_pos.f.u = 700; // (far)
    Obj_Grounder(grounder);
    CHECK_EQ(grounder->collision_flags, 0x02);
    CHECK_EQ(Count(0x8F, 0), 4); // (four bricks)
    player->x_pos.f.u = 540; // (near)
    Obj_Grounder(grounder);
    CHECK_EQ(grounder->routine, 4);
    CHECK_EQ(Count(0x90, 0), 5); // (five rocks)
    for (int pass = 0; pass < 2; pass++) { // (set up, then see the Grounder awake)
        for (int i = 1; i < 0x60; i++) {
            if (sst[i].code == sst_object_codes[0x8F])
                Obj_GrounderWall(&sst[i]);
        }
    }
    int moving = 0;
    for (int i = 0x21; i < 0x60; i++)
        moving += sst[i].code == sst_object_codes[0x8F] && sst[i].routine == 4;
    CHECK_EQ(moving, 4);
}

static void NGHZObjects_TheChopChopSwimsAndSpitsBubblesAndDashesAtSonic(void) {
    Reset();
    Sst *fish = Spawn(0x91, 0, 400, 300);
    player->x_pos.f.u = 1000;
    player->y_pos.f.u = 300;
    Obj_ChopChop(fish);
    CHECK_EQ(fish->x_vel, -0x40);
    CHECK_EQ(fish->collision_flags, 0x02);
    for (int i = 0; i < 20; i++)
        Obj_ChopChop(fish);
    CHECK(fish->x_pos.f.u < 400);
    player->x_pos.f.u = (int16_t)(fish->x_pos.f.u - 0x60); // ahead of it, near
    Obj_ChopChop(fish);
    CHECK_EQ(fish->routine, 4); // (it stops)
    for (int i = 0; i < 0x12; i++)
        Obj_ChopChop(fish);
    CHECK_EQ(fish->routine, 6);
    CHECK_EQ(fish->x_vel, -0x200);
}

static void SSZObjects_TheCrocobotIsSetUpAndFallsUntilItMeetsTheFloor(void) {
    Reset();
    level_id = LEVEL_ID(ZoneId_SSZ, 0);
    Sst *croc = Spawn(0x4E, 0, 400, 300);
    Obj_Crocobot(croc);
    CHECK_EQ(croc->collision_flags, 0x0A);
    CHECK_EQ(croc->width_pixels, 0x10);
    CHECK_EQ(croc->y_radius, 0x10);
    CHECK_EQ(croc->art_tile & 0x7FF, 0x400); // (its art slot, moved to $400 to leave room for the horizontal spikes)
    CHECK(croc->mappings != NULL);
    CHECK_EQ(croc->routine, 0); // (no floor under it here: it keeps falling)
    CHECK(croc->y_vel > 0);
}

static void SSZObjects_TheSwingingPlatformIsDrawnFromTheLevelsTilesWithAChain(void) {
    Reset();
    level_id = LEVEL_ID(ZoneId_SSZ, 0);
    Sst *swing = SpawnSst(0x15, 0x06, 600, 400);
    Obj_SwingDispatch(swing);
    CHECK_EQ(swing->width_pixels, 0x20);
    CHECK_EQ(swing->y_radius, 8);
    CHECK_EQ(swing->art_tile & 0x7FF, 0); // (the level's own tiles: the platform is made of them)
    CHECK(swing->mappings != NULL);
    int chains = 0;
    for (int i = 0; i < SST_COUNT; i++)
        chains += sst[i].code != NULL && sst[i].code == sst_object_codes[0x15] && (sst[i].render_flags & SST_RENDER_MULTI_DRAW);
    CHECK_EQ(chains, 1);
}

static void SSZObjects_TheHangingSwingIsAColumnWithThePlatformBelowIt(void) {
    Reset();
    level_id = LEVEL_ID(ZoneId_SSZ, 0);
    Sst *swing = SpawnSst(0x15, 0x28, 600, 400); // (the pre-beta's kind 2)
    Obj_SwingDispatch(swing);
    CHECK_EQ(swing->y_pos.f.u, 400 + 0x88);
    int columns = 0;
    for (int i = 0; i < SST_COUNT; i++) {
        if (sst[i].code != NULL && sst[i].code == sst_object_codes[0x15] && &sst[i] != swing && sst[i].routine == 4) {
            columns++;
            CHECK_EQ(sst[i].mapping_frame, 3);
            CHECK_EQ(sst[i].y_pos.f.u, 400 + 0x40);
        }
    }
    CHECK_EQ(columns, 1);
    Obj_SwingDispatch(swing); // (and it does not move)
    CHECK_EQ(swing->x_pos.f.u, 600);
}

static void SSZObjects_TheSwitchIsAButtonAndABaseFromTheLevelsTiles(void) {
    Reset();
    level_id = LEVEL_ID(ZoneId_SSZ, 0);
    Sst *sw = Spawn(0x47, 0x00, 400, 300);
    Obj_Switch(sw);
    CHECK((sw->render_flags & SST_RENDER_MULTI_DRAW));
    CHECK_EQ(sw->child_count, 2);
    CHECK_EQ(sw->children[0].frame, 1); // (the base: first in the list, so in front)
    CHECK_EQ(sw->children[1].frame, 0); // (the button, up)
    CHECK_EQ(sw->art_tile >> 13 & 3, 2);    // (palette row 2)
    CHECK_EQ(sw->art_tile & 0x7FF, 0);      // (the level's own tiles: the pieces carry the tile numbers)
}

void RegisterNGHZObjectTests(void) {
    RUN_TEST(NGHZObjects_TheShooterFiresWhenSonicHasBeenNearAndGoes);
    RUN_TEST(NGHZObjects_AnArrowFliesTheWayTheShooterFaces);
    RUN_TEST(NGHZObjects_TheLeavesBoxHurtsNothingAndIsKeyedBySubtype);
    RUN_TEST(NGHZObjects_FastSonicInTheBoxMakesFourLeaves);
    RUN_TEST(NGHZObjects_SlowSonicMakesNoLeaves);
    RUN_TEST(NGHZObjects_TheBossArenaLocksTheCameraAndLoadsTheBossOnce);
    RUN_TEST(NGHZObjects_TheBossWaitsForSonicInTheArenaThenDescendsWithItsLauncher);
    RUN_TEST(NGHZObjects_TheWhispFollowsSonicWithinItsSpeedLimit);
    RUN_TEST(NGHZObjects_TheGrounderBreaksItsWallAndWalksToSonic);
    RUN_TEST(NGHZObjects_TheChopChopSwimsAndSpitsBubblesAndDashesAtSonic);
    RUN_TEST(SSZObjects_TheCrocobotIsSetUpAndFallsUntilItMeetsTheFloor);
    RUN_TEST(SSZObjects_TheSwitchIsAButtonAndABaseFromTheLevelsTiles);
    RUN_TEST(SSZObjects_TheSwingingPlatformIsDrawnFromTheLevelsTilesWithAChain);
    RUN_TEST(SSZObjects_TheHangingSwingIsAColumnWithThePlatformBelowIt);
}
