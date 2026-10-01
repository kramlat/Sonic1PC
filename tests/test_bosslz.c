#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/BossLabyrinth.h"

// Object 77 (LZ boss) flight script, from "_incObj/77 Boss - LZ Main.asm":
// start at (boss_lz_x+$30, boss_lz_y+$500), rise through two fixed corners,
// sway up the shaft to boss_lz_y+$40, move to the top corner
// (boss_lz_x+$16C, boss_lz_y), wait for Sonic, then escape right while the
// camera's right boundary unlocks to boss_lz_end.

#define LZ_BOSS_X 0x1DE0
#define LZ_BOSS_Y 0xC0

static Object *boss;

static void RunBossFrame(void) {
    for (int i = 0; i < LEVEL_OBJECTS; i++)
        if (level_objects[i].type == ObjId_BossLabyrinth)
            Obj_BossLabyrinth(&level_objects[i]);
    frame_count++;
}

static int CountBossParts(void) {
    int n = 0;
    for (int i = 0; i < LEVEL_OBJECTS; i++)
        if (level_objects[i].type == ObjId_BossLabyrinth)
            n++;
    return n;
}

static void SpawnBoss(void) {
    memset(level_objects, 0, sizeof(Object) * LEVEL_OBJECTS);
    memset(player, 0, sizeof(Object));
    player->routine = 2;
    player->pos.l.x.f.u = LZ_BOSS_X - 0x80; // not in the shaft yet
    player->pos.l.y.f.u = LZ_BOSS_Y + 0x500;
    limit_right2 = LZ_BOSS_X + 0x100;
    lock_screen = true;
    frame_count = 0;

    boss = &level_objects[0];
    boss->type = ObjId_BossLabyrinth;
    RunBossFrame();
}

// Run until the ship reaches routine_sec, with Sonic keeping pace just
// below him. Returns frames taken, or -1 on timeout.
static int RunUntil(uint8_t routine_sec, int max_frames) {
    for (int f = 0; f < max_frames; f++) {
        if (boss->routine_sec == routine_sec)
            return f;
        player->pos.l.y.f.u = (int16_t)(boss->pos.l.y.f.u + 0x20);
        RunBossFrame();
    }
    return -1;
}

static void BossLZ_FullFlight(void) {
    SpawnBoss();
    CHECK_EQ(CountBossParts(), 3); // ship, face, flame
    CHECK_EQ(boss->pos.l.x.f.u, LZ_BOSS_X + 0x30);
    CHECK_EQ(boss->pos.l.y.f.u, LZ_BOSS_Y + 0x500);
    CHECK_EQ(boss->col_property, 8);

    // Waits until Sonic enters the shaft.
    for (int i = 0; i < 60; i++)
        RunBossFrame();
    CHECK_EQ(boss->routine_sec, 0);
    CHECK_EQ(boss->pos.l.y.f.u, LZ_BOSS_Y + 0x500);

    player->pos.l.x.f.u = LZ_BOSS_X;
    CHECK(RunUntil(0x8, 3000) >= 0); // climbed the shaft
    Scratch_BossLabyrinth *s = (Scratch_BossLabyrinth *)&boss->scratch;
    // Clamped to boss_lz_y+$40, then moved half a pixel by the new -$80 Y
    // speed in the same frame (real ASM branches to BLZ_MoveBoss after the
    // clamp).
    CHECK_EQ(s->boss_y.f.u, LZ_BOSS_Y + 0x40 - 1);

    CHECK(RunUntil(0xA, 600) >= 0); // reached the top corner
    CHECK_EQ(s->boss_x.f.u, LZ_BOSS_X + 0x16C);
    CHECK_EQ(s->boss_y.f.u, LZ_BOSS_Y);
    CHECK(!boss->status.o.f.x_flip); // facing left, waiting

    // Doesn't leave until Sonic catches up at the top.
    for (int i = 0; i < 60; i++)
        RunBossFrame();
    CHECK_EQ(boss->routine_sec, 0xA);

    player->pos.l.x.f.u = LZ_BOSS_X + 0xE8;
    player->pos.l.y.f.u = LZ_BOSS_Y + 0x30;
    RunBossFrame();
    CHECK_EQ(boss->routine_sec, 0xC);
    CHECK_EQ(s->generic_timer, 50);

    for (int i = 0; i < 50; i++)
        RunBossFrame();
    CHECK_EQ(boss->routine_sec, 0xE);
    CHECK_EQ(boss->xsp, 0x400);

    // Unlocks the right boundary 2px/frame, then deletes itself (and the
    // face/flame) once off screen -- on_screen is never set here since
    // BuildSprites doesn't run.
    uint16_t lim = limit_right2;
    RunBossFrame();
    CHECK_EQ(limit_right2, lim + 2);
    for (int i = 0; i < 400 && CountBossParts() > 0; i++)
        RunBossFrame();
    CHECK_EQ(limit_right2, LZ_BOSS_X + 0x250);
    CHECK_EQ(CountBossParts(), 0);
}

// Climb speed: full within 72px of Sonic, stopped when he's >152px below.
static void BossLZ_ClimbWaitsForSonic(void) {
    SpawnBoss();
    player->pos.l.x.f.u = LZ_BOSS_X;
    CHECK(RunUntil(0x6, 600) >= 0);
    for (int i = 0; i < 30; i++) { // let him start climbing
        player->pos.l.y.f.u = (int16_t)(boss->pos.l.y.f.u + 0x20);
        RunBossFrame();
    }
    player->pos.l.y.f.u = (int16_t)(boss->pos.l.y.f.u + 200);
    int16_t y = boss->pos.l.y.f.u;
    for (int i = 0; i < 30; i++)
        RunBossFrame();
    CHECK_EQ(boss->pos.l.y.f.u, y);
}

// 8 hits before the top: early-defeat flag, 1000 points, trailing explosions.
static void BossLZ_EarlyDefeat(void) {
    SpawnBoss();
    player->pos.l.x.f.u = LZ_BOSS_X;
    CHECK(RunUntil(0x6, 600) >= 0);

    uint32_t before = score;
    boss->status.o.f.flag7 = true; // touch response sets this on the last hit
    boss->col_type = 0;
    RunBossFrame();
    Scratch_BossLabyrinth *s = (Scratch_BossLabyrinth *)&boss->scratch;
    CHECK_EQ(s->early_defeat, 0xFF);
    CHECK(score > before);

    // Flees straight through the wait at the top without Sonic.
    player->pos.l.x.f.u = LZ_BOSS_X - 0x80;
    uint32_t after_first = score;
    CHECK(RunUntil(0xE, 3000) >= 0);
    // Real ASM quirk (kept -- harmless under C): BLZ_Escape1 clears the flag,
    // then falls into BLZ_ShipUpdate the same frame, where the never-cleared
    // defeated bit re-flags it and awards the points a second time. The
    // escape then keeps trailing explosions.
    CHECK_EQ(s->early_defeat, 0xFF);
    CHECK_EQ(score - after_first, after_first - before);
}

void RegisterBossLZTests(void) {
    RUN_TEST(BossLZ_FullFlight);
    RUN_TEST(BossLZ_ClimbWaitsForSonic);
    RUN_TEST(BossLZ_EarlyDefeat);
}
