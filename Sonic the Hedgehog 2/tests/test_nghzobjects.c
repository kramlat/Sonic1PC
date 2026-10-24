#include "test.h"

#include <string.h>

#include "Level.h"
#include "Object.h"
#include "Object/Crocobot.h"
#include "Object/NGHZBadniks.h"
#include "Object/NGHZBoss.h"
#include "Object/BossCommon.h"
#include "Object/NGHZObjects.h"
#include "Object/Sonic.h"

// Neo Green Hill's arrow shooter (22) and leaves (2C), as the prototype's routines have them.

static void Reset(void) {
    memset(objects, 0, sizeof(Object) * 0x60);
    level_id = LEVEL_ID(ZoneId_ARZ, 0);
    scrpos_x.v = scrpos_y.v = 0;
    frame_count = 0;
    player->routine = 2;
    player->pos.l.x.f.u = 0x1000;
    player->pos.l.y.f.u = 0x1000;
}

static Object *Spawn(uint8_t type, uint8_t subtype, int16_t x, int16_t y) {
    Object *obj = &objects[0x20];
    memset(obj, 0, sizeof(Object));
    obj->type = type;
    obj->scratch.u8[0] = subtype;
    obj->pos.l.x.f.u = x;
    obj->pos.l.y.f.u = y;
    return obj;
}

static int Count(uint8_t type, uint8_t routine) {
    int n = 0;
    for (int i = 1; i < 0x60; i++)
        n += objects[i].type == type && objects[i].routine == routine;
    return n;
}

static void NGHZObjects_TheShooterFiresWhenSonicHasBeenNearAndGoes(void) {
    Reset();
    Object *obj = Spawn(0x22, 0, 100, 200);
    for (int i = 0; i < 100; i++)
        Obj_ArrowShooter(obj);
    CHECK_EQ(Count(0x22, 6) + Count(0x22, 8), 0); // (nobody near: no arrow)
    player->pos.l.x.f.u = 120; // within $40: it is ready
    for (int i = 0; i < 40; i++)
        Obj_ArrowShooter(obj);
    CHECK_EQ(Count(0x22, 6) + Count(0x22, 8), 0); // (and still holds its fire)
    player->pos.l.x.f.u = 0x1000; // he leaves: now it draws back and shoots
    int frames = 0;
    while (frames++ < 400 && Count(0x22, 6) + Count(0x22, 8) == 0)
        Obj_ArrowShooter(obj);
    CHECK(Count(0x22, 6) + Count(0x22, 8) >= 1);
}

static void NGHZObjects_AnArrowFliesTheWayTheShooterFaces(void) {
    Reset();
    Object *obj = Spawn(0x22, 0, 100, 200);
    Obj_ArrowShooter(obj);
    Object *arrow = &objects[0x21];
    arrow->type = 0x22;
    arrow->routine = 6;
    arrow->pos.l.x.f.u = 100;
    arrow->pos.l.y.f.u = 200;
    arrow->mappings = obj->mappings;
    Obj_ArrowShooter(arrow);
    CHECK_EQ(arrow->col_type, 0x9B); // (it hurts)
    CHECK_EQ(arrow->xsp, 0x400);
    Object *back = &objects[0x22];
    back->type = 0x22;
    back->routine = 6;
    back->status.o.f.x_flip = true;
    back->pos.l.x.f.u = 100;
    back->pos.l.y.f.u = 200;
    back->mappings = obj->mappings;
    Obj_ArrowShooter(back);
    CHECK_EQ(back->xsp, -0x400);
}

static void NGHZObjects_TheLeavesBoxHurtsNothingAndIsKeyedBySubtype(void) {
    Reset();
    static const uint8_t expected[3] = { 0xD6, 0xD4, 0xD5 }; // (the touch response's "special" kinds, the leaves' boxes)
    for (int sub = 0; sub < 3; sub++) {
        Object *obj = Spawn(0x2C, (uint8_t)sub, 100, 200);
        Obj_NGHZLeaves(obj);
        CHECK_EQ(obj->col_type, expected[sub]);
        CHECK_EQ(obj->width_pixels, 0x80);
    }
}

static void NGHZObjects_FastSonicInTheBoxMakesFourLeaves(void) {
    Reset();
    Object *obj = Spawn(0x2C, 0, 100, 200);
    Obj_NGHZLeaves(obj);
    player->pos.l.x.f.u = 100;
    player->pos.l.y.f.u = 200;
    player->xsp = 0x400; // fast
    obj->col_property = 1; // (the touch response: Sonic is in the box)
    frame_count = 0x10;     // (a frame that is a multiple of 16: Sonic's turn)
    Obj_NGHZLeaves(obj);
    CHECK_EQ(Count(0x2C, 4), 4);
}

static void NGHZObjects_SlowSonicMakesNoLeaves(void) {
    Reset();
    Object *obj = Spawn(0x2C, 0, 100, 200);
    Obj_NGHZLeaves(obj);
    player->xsp = 0x100;
    player->ysp = 0x100;
    obj->col_property = 1;
    frame_count = 0x10;
    Obj_NGHZLeaves(obj);
    CHECK_EQ(Count(0x2C, 4), 0);
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
    Object *boss = Spawn(0x89, 0, 0, 0);
    limit_left2 = 0x2A40 - SCREEN_WIDEADD2;
    player->pos.l.x.f.u = 0x2A00; // (not yet in)
    Obj_NGHZBoss(boss);
    CHECK_EQ(boss->routine, 0);
    player->pos.l.x.f.u = 0x2AD8;
    Obj_NGHZBoss(boss);
    CHECK_EQ(boss->child_count, 3);
    CHECK_EQ(boss->col_type, 0x0F);
    CHECK_EQ(boss->pos.l.x.f.u, 0x2AD8);
    CHECK_EQ(boss->pos.l.y.f.u, 0x388);
    int launchers = 0;
    for (int i = 1; i < 0x60; i++)
        launchers += objects[i].type == 0x89 && objects[i].child_count == 2;
    CHECK_EQ(launchers, 1);
}

static void NGHZObjects_TheWhispFollowsSonicWithinItsSpeedLimit(void) {
    Reset();
    Object *whisp = Spawn(0x8C, 0, 400, 300);
    player->pos.l.x.f.u = 600;
    player->pos.l.y.f.u = 200;
    Obj_Whisp(whisp);
    CHECK_EQ(whisp->col_type, 0x0B);
    for (int i = 0; i < 0x11; i++)
        Obj_Whisp(whisp);
    CHECK_EQ(whisp->routine, 4); // (it has set off)
    for (int i = 0; i < 0x30; i++)
        Obj_Whisp(whisp);
    CHECK(whisp->xsp > 0 && whisp->xsp <= 0x200); // (towards Sonic, on the right)
    CHECK(whisp->ysp < 0 && whisp->ysp >= -0x200); // (and up)
    CHECK(whisp->pos.l.x.f.u > 400);
}

static void NGHZObjects_TheGrounderBreaksItsWallAndWalksToSonic(void) {
    Reset();
    Object *grounder = Spawn(0x8D, 0, 500, 300);
    player->pos.l.x.f.u = 700; // (far)
    Obj_Grounder(grounder);
    CHECK_EQ(grounder->col_type, 0x02);
    CHECK_EQ(Count(0x8F, 0), 4); // (four bricks)
    player->pos.l.x.f.u = 540; // (near)
    Obj_Grounder(grounder);
    CHECK_EQ(grounder->routine, 4);
    CHECK_EQ(Count(0x90, 0), 5); // (five rocks)
    for (int pass = 0; pass < 2; pass++) { // (set up, then see the Grounder awake)
        for (int i = 1; i < 0x60; i++) {
            if (objects[i].type == 0x8F)
                Obj_GrounderWall(&objects[i]);
        }
    }
    int moving = 0;
    for (int i = 0x21; i < 0x60; i++)
        moving += objects[i].type == 0x8F && objects[i].routine == 4;
    CHECK_EQ(moving, 4);
}

static void NGHZObjects_TheChopChopSwimsAndSpitsBubblesAndDashesAtSonic(void) {
    Reset();
    Object *fish = Spawn(0x91, 0, 400, 300);
    player->pos.l.x.f.u = 1000;
    player->pos.l.y.f.u = 300;
    Obj_ChopChop(fish);
    CHECK_EQ(fish->xsp, -0x40);
    CHECK_EQ(fish->col_type, 0x02);
    for (int i = 0; i < 20; i++)
        Obj_ChopChop(fish);
    CHECK(fish->pos.l.x.f.u < 400);
    player->pos.l.x.f.u = (int16_t)(fish->pos.l.x.f.u - 0x60); // ahead of it, near
    Obj_ChopChop(fish);
    CHECK_EQ(fish->routine, 4); // (it stops)
    for (int i = 0; i < 0x12; i++)
        Obj_ChopChop(fish);
    CHECK_EQ(fish->routine, 6);
    CHECK_EQ(fish->xsp, -0x200);
}

static void SSZObjects_TheCrocobotIsSetUpAndFallsUntilItMeetsTheFloor(void) {
    Reset();
    level_id = LEVEL_ID(ZoneId_SSZ, 0);
    Object *croc = Spawn(0x4E, 0, 400, 300);
    Obj_Crocobot(croc);
    CHECK_EQ(croc->col_type, 0x0A);
    CHECK_EQ(croc->width_pixels, 0x10);
    CHECK_EQ(croc->y_rad, 0x10);
    CHECK_EQ(croc->tile & 0x7FF, 0x411);
    CHECK(croc->mappings != NULL);
    CHECK_EQ(croc->routine, 0); // (no floor under it here: it keeps falling)
    CHECK(croc->ysp > 0);
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
}
