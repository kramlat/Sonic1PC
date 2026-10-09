#include "test.h"

#include <string.h>

#include "Game.h"
#include "Level.h"
#include "Object.h"
#include "Object/CharControl.h"
#include "Object/OOZObjects.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"

// Oil Ocean's objects: the oil, the launching platform, the spiked balls, the spring and pusher, the ball on a spring and the cannon.

static void Reset(void) {
    memset(objects, 0, sizeof(Object) * 0x60);
    memset(f_switch, 0, sizeof(f_switch));
    memset(objstate, 0, sizeof(objstate));
    level_id = LEVEL_ID(ZoneId_OOZ, 0);
    scrpos_x.v = scrpos_y.v = 0;
    scrpos_x.f.u = 0;
    limit_btm2 = 0x720;
    debug_use = false;
    jpad2_press = 0;
    player->type = 1;
    player->routine = 2;
    player->y_rad = 19;
    player->pos.l.x.f.u = 0x1000;
    player->pos.l.y.f.u = 0x1000;
    player->status.b = 0;
    OBJ_CONTROL(player) = 0;
}

static Object *Spawn(Object *slot, uint8_t type, uint8_t subtype, int16_t x, int16_t y) {
    memset(slot, 0, sizeof(Object));
    slot->type = type;
    slot->scratch.u8[0] = subtype;
    slot->pos.l.x.f.u = x;
    slot->pos.l.y.f.u = y;
    return slot;
}

#define SLOT(n) (&objects[n])

// Sonic standing on something with its top at `top`
static void StandOn(Object *obj, int16_t x, int16_t top) {
    player->pos.l.x.f.u = x;
    player->pos.l.y.f.u = (int16_t)(top - player->y_rad);
    player->status.p.f.in_air = false;
    player->status.p.f.object_stand = true;
    player->xsp = player->ysp = player->inertia = 0;
    obj->status.b |= 8;
}

// --- 07 ---

static void OOZ_TheOilKillsWhoeverStaysInIt(void) {
    Reset();
    Object *oil = Spawn(SLOT(0x1E), 7, 0, 0, 0);
    Obj_OilSurface(oil);
    CHECK_EQ(oil->pos.l.y.f.u, 0x758);
    StandOn(oil, 300, 0x758 - 0x30);
    for (int i = 0; i < 0x2F; i++) {
        Obj_OilSurface(oil);
        player->routine = 2;
    }
    CHECK_EQ(player->routine, 2); // (still alive)
    CHECK(player->pos.l.y.f.u > 0x758 - 0x30 - 19); // (it has gone down in it)
    for (int i = 0; i < 4; i++)
        Obj_OilSurface(oil);
    CHECK_EQ(player->routine, 6); // (dead)
}

static void OOZ_TheOilReleasesSonicWhenHeLeaves(void) {
    Reset();
    Object *oil = Spawn(SLOT(0x1E), 7, 0, 0, 0);
    Obj_OilSurface(oil);
    StandOn(oil, 300, 0x758 - 0x30);
    for (int i = 0; i < 10; i++)
        Obj_OilSurface(oil);
    CHECK_EQ(oil->scratch.u8[0x10], 0x30 - 10);
    player->status.p.f.in_air = true; // (jumps out, well clear of it)
    player->pos.l.y.f.u -= 0x40;
    for (int i = 0; i < 12; i++) // (one more sink in the frame he leaves, then a pixel a frame back up)
        Obj_OilSurface(oil);
    CHECK_EQ(oil->scratch.u8[0x10], 0x30); // (it comes back up: Sonic is not standing and the depth recovers)
    CHECK_EQ(player->routine, 2);
}

// --- 33 ---

static void OOZ_TheLauncherHopsEverySecondOrSo(void) {
    Reset();
    Object *obj = Spawn(SLOT(0x20), 0x33, 0, 200, 400);
    Obj_OOZLauncher(obj);
    CHECK_EQ(obj->tile & 0x7FF, 0x32C);
    CHECK_EQ(obj->routine_sec, 0); // (its first frame bounces once on its place and settles: waiting)
    CHECK_EQ(obj->pos.l.y.f.u, 400);
    int frames = 0;
    while (frames < 400 && obj->pos.l.y.f.u == 400) {
        Obj_OOZLauncher(obj);
        frames++;
    }
    CHECK(frames >= 0x78 && frames <= 0x7C); // (it leaps after about $78 frames)
    CHECK(obj->pos.l.y.f.u < 400);
    for (int i = 0; i < 400 && obj->routine_sec != 0; i++)
        Obj_OOZLauncher(obj);
    CHECK_EQ(obj->routine_sec, 0); // (it comes down and settles again)
    CHECK_EQ(obj->pos.l.y.f.u, 400);
}

static void OOZ_TheLauncherCarriesWhoStandsInTheMiddleUpAndThrowsThem(void) {
    Reset();
    Object *obj = Spawn(SLOT(0x20), 0x33, 1, 200, 400);
    Obj_OOZLauncher(obj);
    CHECK_EQ(obj->routine_sec, 4);
    StandOn(obj, 200, 400 - 9);
    Obj_OOZLauncher(obj);
    CHECK_EQ(obj->routine_sec, 6); // (it has them)
    CHECK_EQ(OBJ_CONTROL(player), 1);
    for (int i = 0; i < 30 && obj->routine_sec != 8; i++)
        Obj_OOZLauncher(obj);
    CHECK_EQ(obj->routine_sec, 8);
    CHECK_EQ(obj->pos.l.y.f.u, 400 - 0x7D); // (the top of the rise)
    CHECK_EQ(player->ysp, -0x1000);
    CHECK_EQ(player->inertia, 0x800);
    CHECK_EQ(OBJ_CONTROL(player), 0);
    CHECK(player->status.p.f.in_air);
}

static void OOZ_TheLauncherIgnoresSomeoneAtItsEdge(void) {
    Reset();
    Object *obj = Spawn(SLOT(0x20), 0x33, 1, 200, 400);
    Obj_OOZLauncher(obj);
    StandOn(obj, 200 + 0x14, 400 - 9);
    Obj_OOZLauncher(obj);
    CHECK_EQ(obj->routine_sec, 4); // (outside the $10 of its middle: nothing)
}

// --- 43 ---

static void OOZ_ASingleSpikedBallRollsToAndFro(void) {
    Reset();
    Object *obj = Spawn(SLOT(0x20), 0x43, 0, 300, 400);
    scrpos_x.f.u = 200;
    Obj_OOZSpikeball(obj);
    CHECK_EQ(obj->col_type, 0xA5);
    CHECK_EQ(obj->scratch.u8[0x1C - 0x0], obj->scratch.u8[0x1C]); // (its partner is itself)
    int16_t lo = 0x7FFF, hi = -0x7FFF;
    for (int i = 0; i < 1200; i++) {
        Obj_OOZSpikeball(obj);
        if (obj->pos.l.x.f.u < lo) lo = obj->pos.l.x.f.u;
        if (obj->pos.l.x.f.u > hi) hi = obj->pos.l.x.f.u;
    }
    CHECK_EQ(lo, 300 - 0x68);
    CHECK_EQ(hi, 300 + 0x68);
}

static void OOZ_APairOfSpikedBallsTurnRoundWhereTheyMeet(void) {
    Reset();
    Object *obj = Spawn(SLOT(0x20), 0x43, 6, 400, 400);
    scrpos_x.f.u = 300;
    Obj_OOZSpikeball(obj);
    Object *other = SLOT(0x21);
    CHECK_EQ(other->type, 0x43);
    Obj_OOZSpikeball(other); // (the frame's second object)
    CHECK_EQ(obj->pos.l.x.f.u, 400 - 0x18 - 1); // (they move apart a pixel each)
    CHECK_EQ(other->pos.l.x.f.u, 400 + 0x18 + 1);
    int16_t gap_min = 0x7FFF;
    for (int i = 0; i < 3000; i++) {
        Obj_OOZSpikeball(obj);
        Obj_OOZSpikeball(other);
        const int16_t gap = (int16_t)(other->pos.l.x.f.u - obj->pos.l.x.f.u);
        if (gap < gap_min)
            gap_min = gap;
        CHECK(obj->pos.l.x.f.u >= 400 - 0xE8 && obj->pos.l.x.f.u <= 400 + 0xE8);
        CHECK(other->pos.l.x.f.u >= 400 - 0xE8 && other->pos.l.x.f.u <= 400 + 0xE8);
    }
    CHECK(gap_min >= 0x2E); // (they never go into each other)
}

// --- 45 ---

static void OOZ_ThePushSpringIsPressedByStandingAndThrowsOnceFullyPressed(void) {
    Reset();
    Object *obj = Spawn(SLOT(0x20), 0x45, 0, 200, 400);
    Obj_OOZPushSpring(obj);
    CHECK_EQ(obj->scratch.u8[0x8], 0); // (the power's low byte)
    StandOn(obj, 200, 400 - 0x14);
    for (int i = 0; i < 9; i++) {
        Obj_OOZPushSpring(obj);
        CHECK_EQ(obj->frame, i + 1);
    }
    CHECK_EQ(player->ysp, 0);
    Obj_OOZPushSpring(obj); // (fully pressed: it throws)
    CHECK_EQ(player->ysp, -0x1000);
    CHECK(player->status.p.f.in_air);
    CHECK_EQ(player->anim, SonAnimId_Spring);
}

static void OOZ_TheWeakerPushSpringThrowsLessHigh(void) {
    Reset();
    Object *obj = Spawn(SLOT(0x20), 0x45, 2, 200, 400);
    Obj_OOZPushSpring(obj);
    StandOn(obj, 200, 400 - 0x14);
    for (int i = 0; i < 10; i++)
        Obj_OOZPushSpring(obj);
    CHECK_EQ(player->ysp, -0xA00);
}

static void OOZ_ThePusherReturnsToWhereItRests(void) {
    Reset();
    Object *obj = Spawn(SLOT(0x20), 0x45, 0x10, 200, 400);
    Obj_OOZPushSpring(obj);
    CHECK_EQ(obj->routine, 4);
    CHECK_EQ(obj->frame, 0xA);
    obj->pos.l.x.f.u = 192; // (pushed 8 to the left)
    obj->frame = 0xA + 8;
    Obj_OOZPushSpring(obj);
    CHECK_EQ(obj->pos.l.x.f.u, 196); // (back 4 a frame)
    Obj_OOZPushSpring(obj);
    CHECK_EQ(obj->pos.l.x.f.u, 200);
    CHECK_EQ(obj->frame, 0xA);
}

// --- 46 ---

static void OOZ_TheBallSpringsOffWhenItsSwitchIsDown(void) {
    Reset();
    Object *obj = Spawn(SLOT(0x20), 0x46, 0x20, 200, 400);
    Obj_OOZSpringBall(obj);
    CHECK_EQ(obj->routine, 2);
    int springs = 0;
    for (int i = 0x21; i < 0x60; i++)
        springs += objects[i].type == 0x46 && objects[i].routine == 6;
    CHECK_EQ(springs, 1);
    Obj_OOZSpringBall(obj);
    CHECK_EQ(obj->routine, 2); // (waiting)
    f_switch[2] = 1;
    Obj_OOZSpringBall(obj);
    CHECK_EQ(obj->routine, 4);
    CHECK_EQ(obj->ysp, -0x300);
    CHECK_EQ(obj->inertia, 0x100);
}

static void OOZ_TheBallSpringsOffWhenPlayerTwoPressesA(void) {
    Reset();
    Object *obj = Spawn(SLOT(0x20), 0x46, 0, 200, 400);
    Obj_OOZSpringBall(obj);
    jpad2_press = JPAD_A;
    Obj_OOZSpringBall(obj);
    CHECK_EQ(obj->routine, 4);
}

static void OOZ_AFlippedBallGoesTheOtherWay(void) {
    Reset();
    Object *obj = Spawn(SLOT(0x20), 0x46, 0, 200, 400);
    obj->status.o.f.x_flip = true;
    Obj_OOZSpringBall(obj);
    jpad2_press = JPAD_A;
    Obj_OOZSpringBall(obj);
    CHECK_EQ(obj->inertia, -0x100);
}

// --- 48 ---

static void OOZ_TheCannonCatchesACharacterAndFiresItOff(void) {
    Reset();
    Object *obj = Spawn(SLOT(0x20), 0x48, 0, 200, 400);
    Obj_OOZCannon(obj);
    CHECK_EQ(obj->width_pixels, 0x28);
    CHECK_EQ(obj->scratch.u8[0x2C - 0x28], 0);
    player->pos.l.x.f.u = 205;
    player->pos.l.y.f.u = 395;
    Obj_OOZCannon(obj);
    CHECK_EQ(obj->scratch.u8[0x2C - 0x28], 2); // (caught)
    CHECK_EQ(player->pos.l.x.f.u, 200);
    CHECK_EQ(player->pos.l.y.f.u, 400);
    CHECK_EQ(OBJ_CONTROL(player), 0x81);
    CHECK_EQ(player->inertia, 0x1000);
    for (int i = 0; i < 100 && obj->scratch.u8[0x2C - 0x28] == 2; i++)
        Obj_OOZCannon(obj);
    CHECK_EQ(obj->scratch.u8[0x2C - 0x28], 4); // (fired)
    CHECK_EQ(player->xsp, 0x1000); // (subtype 0 fires to the right)
    CHECK_EQ(player->ysp, 0);
    const int16_t x = player->pos.l.x.f.u;
    Obj_OOZCannon(obj);
    CHECK_EQ(player->pos.l.x.f.u, x + 0x10); // ($1000 is 16 pixels a frame)
}

static void OOZ_TheCannonIgnoresACharacterOutOfReach(void) {
    Reset();
    Object *obj = Spawn(SLOT(0x20), 0x48, 0, 200, 400);
    Obj_OOZCannon(obj);
    player->pos.l.x.f.u = 200 + 0x10;
    player->pos.l.y.f.u = 400;
    Obj_OOZCannon(obj);
    CHECK_EQ(obj->scratch.u8[0x2C - 0x28], 0);
}

static void OOZ_ACannonWithBitSevenLetsGoAtOnce(void) {
    Reset();
    Object *obj = Spawn(SLOT(0x20), 0x48, 0x80, 200, 400);
    Obj_OOZCannon(obj);
    player->pos.l.x.f.u = 200;
    player->pos.l.y.f.u = 400;
    Obj_OOZCannon(obj);
    for (int i = 0; i < 100 && obj->scratch.u8[0x2C - 0x28] == 2; i++)
        Obj_OOZCannon(obj);
    CHECK_EQ(obj->scratch.u8[0x2C - 0x28], 6); // (waiting to be ready again)
    CHECK_EQ(OBJ_CONTROL(player), 0);
    CHECK(player->status.p.f.in_air);
}

void RegisterOOZObjectTests(void) {
    RUN_TEST(OOZ_TheOilKillsWhoeverStaysInIt);
    RUN_TEST(OOZ_TheOilReleasesSonicWhenHeLeaves);
    RUN_TEST(OOZ_TheLauncherHopsEverySecondOrSo);
    RUN_TEST(OOZ_TheLauncherCarriesWhoStandsInTheMiddleUpAndThrowsThem);
    RUN_TEST(OOZ_TheLauncherIgnoresSomeoneAtItsEdge);
    RUN_TEST(OOZ_ASingleSpikedBallRollsToAndFro);
    RUN_TEST(OOZ_APairOfSpikedBallsTurnRoundWhereTheyMeet);
    RUN_TEST(OOZ_ThePushSpringIsPressedByStandingAndThrowsOnceFullyPressed);
    RUN_TEST(OOZ_TheWeakerPushSpringThrowsLessHigh);
    RUN_TEST(OOZ_ThePusherReturnsToWhereItRests);
    RUN_TEST(OOZ_TheBallSpringsOffWhenItsSwitchIsDown);
    RUN_TEST(OOZ_TheBallSpringsOffWhenPlayerTwoPressesA);
    RUN_TEST(OOZ_AFlippedBallGoesTheOtherWay);
    RUN_TEST(OOZ_TheCannonCatchesACharacterAndFiresItOff);
    RUN_TEST(OOZ_TheCannonIgnoresACharacterOutOfReach);
    RUN_TEST(OOZ_ACannonWithBitSevenLetsGoAtOnce);
}
