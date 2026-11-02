#include "Object/InvincibilityStars.h"
#include "Object/CNZObjects.h"
#include "Object/CNZFlippers.h"
#include "Object/CNZSlots.h"
#include "Object/CNZPlunger.h"
#include "Object/CharControl.h"
#include "Game.h"
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

static void Stars_TheInvincibilityStarsAreOneObjectOfEightAndGoWithTheInvincibility(void) {
    Reset();
    invincibility = true;
    player->x_pos.f.u = 300;
    player->y_pos.f.u = 200;
    player->status = 0;
    player->angle = 0;
    Sst *stars = SpawnSst(0x35, 0, 0, 0);
    stars->code = Obj_InvincibilityStars;
    Obj_InvincibilityStars(stars);
    CHECK_EQ(stars->child_count, 8);
    CHECK((stars->render_flags & SST_RENDER_MULTI_DRAW));
    CHECK_EQ(stars->x_pos.f.u, 300);
    for (int i = 0; i < 8; i++) { // (all of them near Sonic)
        CHECK(stars->children[i].x > 300 - 0x30 && stars->children[i].x < 300 + 0x30);
        CHECK(stars->children[i].y > 200 - 0x30 && stars->children[i].y < 200 + 0x30);
    }
    invincibility = false;
    Obj_InvincibilityStars(stars);
    CHECK(stars->code == NULL); // (it is gone)
}

static void CNZObjects_TheGreenBumperThrowsAndGoesAtTheThirdHit(void) {
    Reset();
    level_id = LEVEL_ID(ZoneId_CNZ, 0);
    Sst *bumper = SpawnSst(0xD8, 0x00, 500, 300); // (a flat one)
    Obj_CNZGreenBumper(bumper);
    CHECK_EQ(bumper->collision_flags, 0xD7);
    CHECK_EQ(bumper->art_tile >> 13 & 3, 2);
    player->x_pos.f.u = 500;
    player->y_pos.f.u = 260; // (above it)
    for (int hit = 1; hit <= 3; hit++) {
        player->y_vel = 0;
        bumper->collision_property = 1;
        Obj_CNZGreenBumper(bumper);
        CHECK_EQ(player->y_vel, -0x700); // (up and away)
        CHECK((player->status & SST_PSTATUS_IN_AIR) != 0);
        for (int i = 0; i < 6; i++)
            Obj_CNZGreenBumper(bumper); // (the wait of 4 frames)
        if (hit < 3)
            CHECK_EQ(bumper->art_tile >> 13 & 3, 2 - hit); // (a colour less each time)
    }
    CHECK_EQ(bumper->routine, 4); // (at the third it goes)
}

static void CNZObjects_TheFlipperThrowsWhoStandsOnItWhenAButtonIsPressed(void) {
    Reset();
    level_id = LEVEL_ID(ZoneId_CNZ, 0);
    Sst *flipper = SpawnSst(0x86, 0x00, 500, 300);
    Obj_CNZFlipper(flipper);
    CHECK_EQ(flipper->routine, 2);
    player->x_pos.f.u = 520;
    player->y_pos.f.u = 280;
    flipper->status |= 0x08; // (Sonic stands on it)
    player->status |= SST_PSTATUS_OBJECT_STAND;
    jpad1_press2 = 0;
    Obj_CNZFlipper(flipper); // (a frame on it)
    jpad1_press2 = 0x40;     // (a press)
    Obj_CNZFlipper(flipper);
    jpad1_press2 = 0;
    Obj_CNZFlipper(flipper);
    CHECK((player->status & SST_PSTATUS_IN_AIR) != 0);
    CHECK(player->y_vel < 0); // (thrown up)
}

static void CNZObjects_TheElevatorRidesToTheOtherEndAndBack(void) {
    Reset();
    level_id = LEVEL_ID(ZoneId_CNZ, 0);
    Sst *lift = SpawnSst(0xD5, 0x10, 900, 500); // (a run of $40 each way)
    Obj_CNZElevator(lift);
    CHECK_EQ(lift->y_pos.f.u, 500 - 0x40); // (at the upper end)
    int frames = 0;
    while (lift->y_pos.f.u != 500 + 0x40 && frames++ < 400) {
        lift->status |= 0x08; // (Sonic stands on it)
        Obj_CNZElevator(lift);
    }
    CHECK_EQ(lift->y_pos.f.u, 500 + 0x40); // (the lower end, where it has come to a stop)
    frames = 0;
    lift->status &= (uint8_t)~0x18;
    while (lift->y_pos.f.u != 500 - 0x40 && frames++ < 400)
        Obj_CNZElevator(lift);
    CHECK_EQ(lift->y_pos.f.u, 500 - 0x40); // (back, once he has left)
}

static void CNZObjects_ThePlungerHoldsDrawsBackAndThrowsUp(void) {
    Reset();
    level_id = LEVEL_ID(ZoneId_CNZ, 0);
    Sst *plunger = SpawnSst(0x85, 0x00, 700, 400);
    Obj_CNZPlunger(plunger);
    CHECK((plunger->render_flags & SST_RENDER_MULTI_DRAW));
    CHECK_EQ(plunger->child_count, 1);
    const int16_t head_y = plunger->y_pos.f.u;
    player->x_pos.f.u = 700;
    player->y_pos.f.u = head_y - 0x40;
    player->y_vel = 0x200;
    player->status |= SST_PSTATUS_IN_AIR;
    for (int i = 0; i < 80 && OBJ_CONTROL(player) != 0x81; i++) {
        player->y_pos.f.u += player->y_vel >> 8;
        Obj_CNZPlunger(plunger);
    }
    CHECK_EQ(OBJ_CONTROL(player), 0x81); // (held)
    jpad1_hold2 = 0x40; // (a button held)
    for (int i = 0; i < 80; i++)
        Obj_CNZPlunger(plunger);
    CHECK(plunger->y_pos.f.u > head_y + 0x10); // (drawn back)
    jpad1_hold2 = 0;
    for (int i = 0; i < 3; i++)
        Obj_CNZPlunger(plunger);
    CHECK(player->y_vel < -0x1000); // (thrown up)
    CHECK_EQ(OBJ_CONTROL(player), 0);
}

static void CNZObjects_TheBlueBumperThrowsBySideAndMovesWithASubtype(void) {
    Reset();
    level_id = LEVEL_ID(ZoneId_CNZ, 0);
    Sst *bumper = SpawnSst(0xD7, 0x00, 500, 300);
    Obj_CNZBlueBumper(bumper);
    CHECK_EQ(bumper->collision_flags, 0xCA);
    player->x_pos.f.u = 460; // (to its left)
    player->y_pos.f.u = 300;
    player->x_vel = 0;
    bumper->collision_property = 1;
    Obj_CNZBlueBumper(bumper);
    CHECK_EQ(player->x_vel, -0x800); // (away from it)
    player->x_pos.f.u = 540; // (to its right)
    bumper->collision_property = 1;
    Obj_CNZBlueBumper(bumper);
    CHECK_EQ(player->x_vel, 0x800);
    Sst *mover = SpawnSst(0xD7, 0x01, 800, 300);
    Obj_CNZBlueBumper(mover);
    const int16_t x = mover->x_pos.f.u;
    Obj_CNZBlueBumper(mover);
    CHECK_EQ(mover->x_pos.f.u, x + 1); // (a pixel a frame)
}

static void CNZObjects_TheSlotMachineSpinsAndStopsOnItsOwn(void) {
    Reset();
    SlotMachine_Reset();
    for (int i = 0; i < 4; i++)
        SlotMachine_Process();
    CHECK_EQ(SlotMachine_State(), 0x38); // (set up and resting)
    SlotMachine_Start();
    CHECK_EQ(SlotMachine_State(), 0x08);
    frame_count = 0x1234;
    int frames = 0;
    while (SlotMachine_State() != 0x38 && frames < 6000) {
        frame_count++;
        SlotMachine_Process();
        frames++;
    }
    CHECK_EQ(SlotMachine_State(), 0x38); // (all three reels have stopped)
    CHECK(frames > 100 && frames < 3000);
}

static void CNZObjects_TheSlotStarterHoldsACharacterAndLetsHimGoDown(void) {
    Reset();
    level_id = LEVEL_ID(ZoneId_CNZ, 0);
    SlotMachine_Reset();
    for (int i = 0; i < 4; i++)
        SlotMachine_Process();
    Sst *starter = SpawnSst(0xD6, 0x00, 600, 400);
    Obj_CNZSlotStarter(starter);
    CHECK_EQ(starter->mapping_frame, 0);
    // (a character who falls onto it)
    player->x_pos.f.u = 600;
    player->y_pos.f.u = 400 - 0x50;
    player->y_vel = 0x200;
    player->status |= SST_PSTATUS_IN_AIR;
    for (int i = 0; i < 60 && OBJ_CONTROL(player) != 0x81; i++) {
        player->y_pos.f.u += player->y_vel >> 8;
        Obj_CNZSlotStarter(starter);
    }
    CHECK_EQ(OBJ_CONTROL(player), 0x81); // (held)
    CHECK_EQ(SlotMachine_State(), 0x08);  // (and the reels start)
    for (int i = 0; i < 0x7A; i++)
        Obj_CNZSlotStarter(starter);
    CHECK_EQ(OBJ_CONTROL(player), 0);
    CHECK_EQ(player->y_vel, 0x400);
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
    RUN_TEST(CNZObjects_TheGreenBumperThrowsAndGoesAtTheThirdHit);
    RUN_TEST(CNZObjects_TheElevatorRidesToTheOtherEndAndBack);
    RUN_TEST(CNZObjects_ThePlungerHoldsDrawsBackAndThrowsUp);
    RUN_TEST(CNZObjects_TheBlueBumperThrowsBySideAndMovesWithASubtype);
    RUN_TEST(CNZObjects_TheSlotMachineSpinsAndStopsOnItsOwn);
    RUN_TEST(CNZObjects_TheSlotStarterHoldsACharacterAndLetsHimGoDown);
    RUN_TEST(CNZObjects_TheFlipperThrowsWhoStandsOnItWhenAButtonIsPressed);
    RUN_TEST(Stars_TheInvincibilityStarsAreOneObjectOfEightAndGoWithTheInvincibility);
}
