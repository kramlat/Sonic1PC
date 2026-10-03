#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/BossStarLight.h"
#include "Object/Seesaw.h"

// Objects 7A/7B (SLZ boss + spike balls), from "_incObj/7A, 7B Boss - SLZ
// Main and Spike Balls.asm". boss_slz_x = 0x2000, boss_slz_y = 0x210.

#define SLZ_X 0x2000
#define SLZ_Y 0x210

static Object *boss;
static Object *saw;

static void RunFrame(void) {
    for (int i = 0; i < LEVEL_OBJECTS; i++) {
        Object *o = &level_objects[i];
        if (o->type == ObjId_BossStarLight)
            Obj_BossStarLight(o);
        else if (o->type == ObjId_BossSpikeball)
            Obj_BossSpikeball(o);
    }
    frame_count++;
}

static int Count(uint8_t type) {
    int n = 0;
    for (int i = 0; i < LEVEL_OBJECTS; i++)
        if (level_objects[i].type == type)
            n++;
    return n;
}

static Object *FindBall(void) {
    for (int i = 0; i < LEVEL_OBJECTS; i++)
        if (level_objects[i].type == ObjId_BossSpikeball)
            return &level_objects[i];
    return NULL;
}

static void Spawn(void) {
    memset(level_objects, 0, sizeof(Object) * LEVEL_OBJECTS);
    memset(player, 0, sizeof(Object));
    player->routine = 2;
    player->pos.l.x.f.u = SLZ_X;
    player->pos.l.y.f.u = SLZ_Y;
    limit_right2 = SLZ_X + 0x100;
    lock_screen = true;
    boss_status = 0;
    scrpos_x.f.u = SLZ_X + 0x80; // keeps objects in range of the camera
    frame_count = 0;

    // Three boss-fight seesaws (subtype != 0), flat, at the arena's floor.
    static const int16_t xs[3] = { 0x20F0, 0x2160, 0x21D0 };
    for (int i = 0; i < 3; i++) {
        Object *s = &level_objects[10 + i];
        s->type = ObjId_Seesaw;
        s->scratch.u8[0] = 1;
        s->frame = 1;
        s->pos.l.x.f.u = xs[i];
        s->pos.l.y.f.u = 0x2F0;
    }
    saw = &level_objects[10];

    boss = &level_objects[0];
    boss->type = ObjId_BossStarLight;
    RunFrame();
}

static int RunUntil(uint8_t routine_sec, int max) {
    for (int f = 0; f < max; f++) {
        if (boss->routine_sec == routine_sec)
            return f;
        RunFrame();
    }
    return -1;
}

static void BossSLZ_Spawn(void) {
    Spawn();
    CHECK_EQ(Count(ObjId_BossStarLight), 4); // ship, face, flame, pipe
    CHECK_EQ(boss->pos.l.x.f.u, SLZ_X + 0x188 - 1); // 1px left after the first frame at -0x100
    CHECK_EQ(boss->col_property, 8);
    Scratch_BossStarLight *s = (Scratch_BossStarLight *)&boss->scratch;
    CHECK_EQ(s->seesaw_count, 3);
    CHECK_EQ(boss->routine_sec, 0);
}

static void BossSLZ_DropsBallOnSeesaw(void) {
    Spawn();
    CHECK(RunUntil(0x2, 400) >= 0); // reached the left bound, pacing
    CHECK(RunUntil(0x4, 400) >= 0); // lined up over a seesaw's drop point
    Scratch_BossStarLight *s = (Scratch_BossStarLight *)&boss->scratch;
    CHECK_EQ(s->generic_timer, 40);
    CHECK(s->pick >= 0);
    CHECK(Count(ObjId_BossSpikeball) == 0);

    RunFrame();
    Object *ball = FindBall();
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
    CHECK(ball->pos.l.y.f.u >= 0x2F0 - 0x2F && ball->pos.l.y.f.u <= 0x2F0 - 8);
    // Only one ball per seesaw.
    for (int i = 0; i < 300; i++)
        RunFrame();
    int balls = 0;
    for (int i = 0; i < LEVEL_OBJECTS; i++)
        if (level_objects[i].type == ObjId_BossSpikeball && level_objects[i].routine != 0xA)
            balls++;
    CHECK(balls <= 3);
}

// Final hit: defeated flag -> explode -> recover -> escape -> delete.
static void BossSLZ_DefeatAndEscape(void) {
    Spawn();
    uint32_t before = score;
    boss->status.o.f.flag7 = true;
    RunFrame();
    CHECK_EQ(boss->routine_sec, 6);
    CHECK_EQ(score - before, 100); // 1000 points: the score counts in tens

    CHECK(RunUntil(0x8, 400) >= 0);
    CHECK(boss->status.o.f.x_flip);
    CHECK(!boss->status.o.f.flag7);
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
    Object *ball = &level_objects[30];
    ball->type = ObjId_BossSpikeball;
    ball->routine = 6;
    ball->pos.l.x.f.u = boss->pos.l.x.f.u;
    ball->pos.l.y.f.u = boss->pos.l.y.f.u;
    ball->ysp = -0x100;
    Scratch_BossSpikeball *bs = (Scratch_BossSpikeball *)&ball->scratch;
    bs->seesaw_index = (uint8_t)(saw - objects);
    bs->base_x = saw->pos.l.x.f.u;
    bs->seesaw_y = 0x2F0;
    bs->timer = 32;
    RunFrame();
    CHECK_EQ(boss->col_property, 7);
    CHECK_EQ(ball->routine, 8);
}

// A ball that's counted down explodes into 4 fragments that fly off and clean up.
static void BossSLZ_ExplodeFragments(void) {
    Spawn();
    Object *ball = &level_objects[30];
    ball->type = ObjId_BossSpikeball;
    ball->routine = 8;
    ball->pos.l.x.f.u = 0x20F0 + 0x28;
    ball->pos.l.y.f.u = 0x2D0;
    Scratch_BossSpikeball *bs = (Scratch_BossSpikeball *)&ball->scratch;
    bs->timer = 32;
    bs->base_x = 0x20F0;
    bs->seesaw_y = 0x2F0;
    RunFrame();
    CHECK_EQ(ball->type, ObjId_ExplosionBomb);
    int frags = 0;
    for (int i = 0; i < LEVEL_OBJECTS; i++)
        if (level_objects[i].type == ObjId_BossSpikeball && level_objects[i].routine == 0xA)
            frags++;
    CHECK_EQ(frags, 4);
    for (int i = 0; i < 10; i++)
        RunFrame();
    for (int i = 0; i < LEVEL_OBJECTS; i++)
        if (level_objects[i].type == ObjId_BossSpikeball && level_objects[i].routine == 0xA)
            CHECK(level_objects[i].ysp > -0x340 + 0x18 * 5);
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
    for (int i = 0; i < 3; i++) {
        Object *s = &level_objects[10 + i];
        s->scratch.u8[0] = 0xFF;
        s->pos.l.x.f.u = (int16_t)(0x203C + i * 0x64);
        s->pos.l.y.f.u = 0x2D4;
    }
    player->type = 1;
    player->x_rad = 9; player->y_rad = 19;
    player->pos.l.x.f.u = 0x20A0 - 20;
    player->pos.l.y.f.u = 0x2A0;
    player->status.p.f.in_air = true;
    alarm(10);
    for (int f = 0; f < 3000; f++) {
        for (int i = 10; i < 13; i++)
            Obj_Seesaw(&level_objects[i]);
        player->ysp += 0x38;
        RunFrame();
    }
    alarm(0);
    CHECK(1);
}


#include "Object/LavaBall.h"
static void BossSLZ_OffscreenDeletes(void) {
    memset(level_objects, 0, sizeof(Object) * LEVEL_OBJECTS);
    scrpos_x.f.u = 0x1000;
    int16_t xs[] = { 0x1000 + 320 + 200, 0x1000 + 700, 0x1000 + 900, 0x1000 - 400, 0x1000 - 700 };
    for (int k = 0; k < 5; k++) {
        Object *o = &level_objects[5];
        memset(o, 0, sizeof(Object));
        o->type = ObjId_LavaBall;
        o->scratch.u8[0] = 8;
        o->pos.l.x.f.u = xs[k];
        o->pos.l.y.f.u = 0x100;
        Obj_LavaBall(o);
        Obj_LavaBall(o);
        printf("\n x=%+d from cam -> type %d", xs[k] - 0x1000, o->type);
    }
}
