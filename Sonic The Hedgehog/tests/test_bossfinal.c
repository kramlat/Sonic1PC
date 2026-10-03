#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object.h"
#include "Object/BossFinal.h"
#include "PLC.h"

// Final Zone boss (objects 85/84/86), from s1disasm's "_incObj/85,84,86 Boss - FZ Main, Cylinders, and Plasma
// Balls.asm". boss_fz_x = 0x2450, boss_fz_y = 0x510.

#define FZ_X 0x2450
#define FZ_Y 0x510

static Object *boss;

static int Count(uint8_t type) {
    int n = 0;
    for (int i = 0; i < LEVEL_OBJECTS; i++)
        if (level_objects[i].type == type)
            n++;
    return n;
}

static void RunFrame(void) {
    for (int i = 0; i < LEVEL_OBJECTS; i++) {
        Object *o = &level_objects[i];
        if (o->type == ObjId_BossFinal)
            Obj_BossFinal(o);
        else if (o->type == ObjId_EggmanCylinder)
            Obj_EggmanCylinder(o);
        else if (o->type == ObjId_BossPlasma)
            Obj_BossPlasma(o);
        o = &level_objects[i];
        if (o->type != 0) // BuildSprites would flag everything drawn as on screen
            o->render.f.on_screen = true;
    }
    frame_count++;
}

static Scratch_BossFinal *S(void) { return (Scratch_BossFinal *)&boss->scratch; }

static void Spawn(void) {
    memset(level_objects, 0, sizeof(Object) * LEVEL_OBJECTS);
    memset(player, 0, sizeof(Object));
    player->routine = 2;
    player->pos.l.x.f.u = FZ_X + 0x20; // far from the action (left of the cylinders)
    player->pos.l.y.f.u = FZ_Y + 0x60;
    plc_buffer[0].art = NULL;
    scrpos_x.f.u = FZ_X;
    limit_right2 = FZ_X + 0x100;
    lock_ctrl = false;
    dle_routine = 6;
    random_seed.v = 0x12345678;
    ending_eggmobile_exploding = 1; // stale value from a previous fight

    boss = &level_objects[0];
    boss->type = ObjId_BossFinal;
    RunFrame();
}

static int RunUntilPhase(uint8_t phase, int max) {
    for (int f = 0; f < max; f++) {
        if (S()->link == phase)
            return f;
        RunFrame();
    }
    return -1;
}

static void BossFinal_SpawnsItsParts(void) {
    Spawn();
    CHECK_EQ(Count(ObjId_BossFinal), 6);      // Eggman, panel, legs, cockpit, empty ship, flame
    CHECK_EQ(Count(ObjId_EggmanCylinder), 4);
    CHECK_EQ(Count(ObjId_BossPlasma), 1);
    CHECK_EQ(boss->col_property, 8);
    CHECK_EQ(ending_eggmobile_exploding, 0); // a new fight starts with the flag cleared
}

static void BossFinal_CylindersThenPlasmaThenCylinders(void) {
    Spawn();
    CHECK(RunUntilPhase(2, 20) >= 0); // arena reached: starts the cylinder attack

    // Two cylinders extend (one holds Eggman), then retract; then the plasma attack.
    CHECK(RunUntilPhase(4, 900) >= 0);
    // Plasma: the launcher fires 4 balls; when they're gone the boss goes back to the cylinders.
    int balls_seen = 0;
    for (int f = 0; f < 1500 && S()->link == 4; f++) {
        RunFrame();
        int n = Count(ObjId_BossPlasma) - 1;
        if (n > balls_seen)
            balls_seen = n;
    }
    CHECK_EQ(balls_seen, 4);
    CHECK_EQ(S()->link, 2);
    CHECK_EQ(boss->col_property, 8); // nobody touched him
}

static void BossFinal_DefeatEscapeAndTheEndingFlag(void) {
    Spawn();
    RunUntilPhase(2, 20);
    boss->col_property = 0; // defeated by Sonic's hits
    CHECK(RunUntilPhase(6, 900) >= 0); // cylinders finish, then he falls out of his hiding place
    CHECK_EQ(boss->pos.l.x.f.u, FZ_X + 0x170);

    uint8_t dle = dle_routine;
    player->pos.l.x.f.u = FZ_X + 0x180;
    CHECK(RunUntilPhase(8, 200) >= 0); // lands
    CHECK_EQ(dle_routine, dle + 2);    // lets the level scroll on
    CHECK(RunUntilPhase(0xA, 600) >= 0); // runs to the ship
    CHECK(RunUntilPhase(0xC, 200) >= 0); // jumps in
    CHECK_EQ(boss->col_property, 1);     // one hit left in him
    CHECK(RunUntilPhase(0xE, 400) >= 0); // takes off
    CHECK_EQ(boss->col_type, 0x0F);
    CHECK_EQ(ending_eggmobile_exploding, 0);

    // Sonic hits the fleeing ship: the touch response clears its collision and flags it defeated.
    boss->col_type = 0;
    boss->status.o.f.flag7 = true;
    for (int i = 0; i < 40; i++)
        RunFrame();
    CHECK_EQ(ending_eggmobile_exploding, 1); // the ending gets its exploding Eggmobile
}

static void BossFinal_FleeingShipThatIsNotHitLeavesNormally(void) {
    Spawn();
    boss->col_property = 0;
    for (int f = 0; f < 3000 && S()->link != 0xE; f++) {
        if (S()->link >= 6) // he waits for Sonic to keep up while running to the ship
            player->pos.l.x.f.u = (int16_t)(boss->pos.l.x.f.u - 0x20);
        RunFrame();
    }
    CHECK_EQ(S()->link, 0xE);
    for (int i = 0; i < 200; i++) // never hit: no ending flag
        RunFrame();
    CHECK_EQ(ending_eggmobile_exploding, 0);
}

void RegisterBossFinalTests(void) {
    RUN_TEST(BossFinal_SpawnsItsParts);
    RUN_TEST(BossFinal_CylindersThenPlasmaThenCylinders);
    RUN_TEST(BossFinal_DefeatEscapeAndTheEndingFlag);
    RUN_TEST(BossFinal_FleeingShipThatIsNotHitLeavesNormally);
}
