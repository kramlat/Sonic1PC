#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/BossStarLight.h"
#include "Object/Seesaw.h"
#include "SstLoad.h"

// Objects 7A/7B (SLZ boss + spike balls), from "_incObj/7A, 7B Boss - SLZ
// Main and Spike Balls.asm". boss_slz_x = 0x2000, boss_slz_y = 0x210.

#define SLZ_X 0x2000
#define SLZ_Y 0x210

static Sst *boss;
static Sst *saw;

static void RunFrame(void) {
    for (int i = 0; i < SST_COUNT; i++) {
        Sst *o = &sst[i];
        if (o->code == NULL)
            continue;
        if (o->code == sst_object_codes[ObjId_BossStarLight])
            Obj_BossStarLight(o);
        else if (o->code == sst_object_codes[ObjId_BossSpikeball])
            Obj_BossSpikeball(o);
    }
    frame_count++;
}

static int Count(uint8_t type) {
    int n = 0;
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code != NULL && sst[i].code == sst_object_codes[type])
            n++;
    return n;
}

static Sst *FindBall(void) {
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code != NULL && sst[i].code == sst_object_codes[ObjId_BossSpikeball])
            return &sst[i];
    return NULL;
}

static void Spawn(void) {
    Sst_Reset();
    memset(player, 0, sizeof(Sst));
    static const SstObjectIndex index_ = {sst_object_codes, 0x100};
    SstLoadCamera camera = {SLZ_X + 0x80, 0, false, 0xFFF};
    SstLoad_Init(&index_, NULL, false, &camera);
    player->routine = 2;
    player->x_pos.f.u = SLZ_X;
    player->y_pos.f.u = SLZ_Y;
    limit_right2 = SLZ_X + 0x100;
    lock_screen = true;
    boss_status = 0;
    scrpos_x.f.u = SLZ_X + 0x80; // keeps objects in range of the camera
    frame_count = 0;

    // Three boss-fight seesaws (subtype != 0), flat, at the arena's floor.
    static const int16_t xs[3] = { 0x20F0, 0x2160, 0x21D0 };
    for (int i = 0; i < 3; i++) {
        Sst *s = Sst_Spawn(ObjId_Seesaw, xs[i], 0x2F0);
        s->subtype = 1;
        s->mapping_frame = 1;
        if (i == 0)
            saw = s;
    }

    boss = Sst_Spawn(ObjId_BossStarLight, 0, 0);
    RunFrame();
}

static int RunUntil(uint8_t routine_sec, int max) {
    for (int f = 0; f < max; f++) {
        if (boss->routine_secondary == routine_sec)
            return f;
        RunFrame();
    }
    return -1;
}

static void BossSLZ_Spawn(void) {
    Spawn();
    CHECK_EQ(Count(ObjId_BossStarLight), 4); // ship, face, flame, pipe
    CHECK_EQ(boss->x_pos.f.u, SLZ_X + 0x188 - 1); // 1px left after the first frame at -0x100
    CHECK_EQ(boss->collision_property, 8);
    Own_BossStarLight *s = (Own_BossStarLight *)boss->own.u8;
    CHECK_EQ(s->seesaw_count, 3);
    CHECK_EQ(boss->routine_secondary, 0);
}

static void BossSLZ_DropsBallOnSeesaw(void) {
    Spawn();
    CHECK(RunUntil(0x2, 400) >= 0); // reached the left bound, pacing
    CHECK(RunUntil(0x4, 400) >= 0); // lined up over a seesaw's drop point
    Own_BossStarLight *s = (Own_BossStarLight *)boss->own.u8;
    CHECK_EQ(s->generic_timer, 40);
    CHECK(s->pick >= 0);
    CHECK(Count(ObjId_BossSpikeball) == 0);

    RunFrame();
    Sst *ball = FindBall();
    CHECK(ball != NULL);
    if (ball == NULL)
        return;
    CHECK_EQ(Count(ObjId_BossSpikeball), 1);

    // The ball falls onto the seesaw and sits there flickering.
    for (int i = 0; i < 120; i++)
        RunFrame();
    ball = FindBall();
    CHECK(ball != NULL);
    if (ball == NULL)
        return;
    CHECK_EQ(ball->routine, 4);
    CHECK(ball->y_pos.f.u >= 0x2F0 - 0x2F && ball->y_pos.f.u <= 0x2F0 - 8);
    // Only one ball per seesaw.
    for (int i = 0; i < 300; i++)
        RunFrame();
    int balls = 0;
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code != NULL && sst[i].code == sst_object_codes[ObjId_BossSpikeball] && sst[i].routine != 0xA)
            balls++;
    CHECK(balls <= 3);
}

// Final hit: defeated flag -> explode -> recover -> escape -> delete.
static void BossSLZ_DefeatAndEscape(void) {
    Spawn();
    uint32_t before = score;
    boss->status |= 0x80;
    RunFrame();
    CHECK_EQ(boss->routine_secondary, 6);
    CHECK_EQ(score - before, 100); // 1000 points: the score counts in tens

    CHECK(RunUntil(0x8, 400) >= 0);
    CHECK((boss->status & SST_STATUS_X_FLIP));
    CHECK(!(boss->status & 0x80));
    CHECK_EQ(boss_status, 1);

    CHECK(RunUntil(0xA, 400) >= 0);
    uint16_t lim = limit_right2;
    RunFrame();
    CHECK_EQ(limit_right2, lim + 2);
    for (int i = 0; i < 400 && Count(ObjId_BossStarLight) > 0; i++)
        RunFrame();
    CHECK_EQ(limit_right2, SLZ_X + 0x160);
    CHECK_EQ(Count(ObjId_BossStarLight), 0);
}

// A bounced ball hitting Eggman's box takes a hit and starts the flash.
static void BossSLZ_BallHitsBoss(void) {
    Spawn();
    RunUntil(0x2, 400);
    Sst *ball = Sst_Spawn(ObjId_BossSpikeball, boss->x_pos.f.u, boss->y_pos.f.u);
    ball->routine = 6;
    ball->y_vel = -0x100;
    Own_BossSpikeball *bs = (Own_BossSpikeball *)ball->own.u8;
    bs->seesaw_index = (uint8_t)(saw - sst);
    bs->base_x = saw->x_pos.f.u;
    bs->seesaw_y = 0x2F0;
    bs->timer = 32;
    RunFrame();
    CHECK_EQ(boss->collision_property, 7);
    CHECK_EQ(ball->routine, 8);
}

// A ball that's counted down explodes into 4 fragments that fly off and clean up.
static void BossSLZ_ExplodeFragments(void) {
    Spawn();
    Sst *ball = Sst_Spawn(ObjId_BossSpikeball, 0x20F0 + 0x28, 0x2D0);
    ball->routine = 8;
    Own_BossSpikeball *bs = (Own_BossSpikeball *)ball->own.u8;
    bs->timer = 32;
    bs->base_x = 0x20F0;
    bs->seesaw_y = 0x2F0;
    RunFrame();
    CHECK(ball->code == sst_object_codes[ObjId_ExplosionBomb]);
    int frags = 0;
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code != NULL && sst[i].code == sst_object_codes[ObjId_BossSpikeball] && sst[i].routine == 0xA)
            frags++;
    CHECK_EQ(frags, 4);
    for (int i = 0; i < 10; i++)
        RunFrame();
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code != NULL && sst[i].code == sst_object_codes[ObjId_BossSpikeball] && sst[i].routine == 0xA)
            CHECK(sst[i].y_vel > -0x340 + 0x18 * 5);
}

static void BossSLZ_RealSeesawsDontHang(void);
static void BossSLZ_OffscreenDeletes(void);
void RegisterBossSLZTests(void) {
    RUN_TEST(BossSLZ_Spawn);
    RUN_TEST(BossSLZ_DropsBallOnSeesaw);
    RUN_TEST(BossSLZ_DefeatAndEscape);
    RUN_TEST(BossSLZ_BallHitsBoss);
    RUN_TEST(BossSLZ_ExplodeFragments);
    RUN_TEST(BossSLZ_RealSeesawsDontHang);
    RUN_TEST(BossSLZ_OffscreenDeletes);
}

#include <unistd.h>
#include "Object/Sonic.h"

static void BossSLZ_RealSeesawsDontHang(void) {
    Spawn();
    Sst *saws[3];
    for (int i = 0, n = 0; i < SST_COUNT && n < 3; i++)
        if (sst[i].code != NULL && sst[i].code == sst_object_codes[ObjId_Seesaw])
            saws[n++] = &sst[i];
    for (int i = 0; i < 3; i++) {
        Sst *s = saws[i];
        s->subtype = 0xFF;
        s->x_pos.f.u = (int16_t)(0x203C + i * 0x64);
        s->y_pos.f.u = 0x2D4;
    }
    player->code = sst_object_codes[1];
    player->x_radius = 9; player->y_radius = 19;
    player->x_pos.f.u = 0x20A0 - 20;
    player->y_pos.f.u = 0x2A0;
    player->status |= SST_PSTATUS_IN_AIR;
    alarm(10);
    for (int f = 0; f < 3000; f++) {
        for (int i = 0; i < 3; i++)
            Obj_Seesaw(saws[i]);
        player->y_vel += 0x38;
        RunFrame();
    }
    alarm(0);
    CHECK(1);
}


#include "Object/LavaBall.h"
static void BossSLZ_OffscreenDeletes(void) {
    scrpos_x.f.u = 0x1000;
    int16_t xs[] = { 0x1000 + 320 + 200, 0x1000 + 700, 0x1000 + 900, 0x1000 - 400, 0x1000 - 700 };
    static const int expect_alive[] = { 1, 0, 0, 0, 0 }; // (only within the 0x280 of the coarse back edge, 0x80 left of the camera)
    for (int k = 0; k < 5; k++) {
        Sst_Reset();
        SstLoadCamera camera = {0x1000, 0, false, 0xFFF};
        static const SstObjectIndex index_ = {sst_object_codes, 0x100};
        SstLoad_Init(&index_, NULL, false, &camera);
        Sst *o = Sst_Spawn(ObjId_LavaBall, xs[k], 0x100);
        o->subtype = 8;
        o->code(o);
        if (o->code != NULL)
            o->code(o);
        CHECK_EQ(o->code != NULL, expect_alive[k]);
    }
}
