#include "test.h"

#include <string.h>

#include "Game.h"
#include "Level.h"
#include "Object.h"
#include "Object/CharControl.h"
#include "Object/CPZObjects.h"
#include "Object/DHZObjects.h"
#include "Object/MTZObjects.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Oscillatory Routines.h"
#include "Solid.h"

// Metropolis' objects: the teleporter, the platforms that go round a path, the machinery, the elevators, the gears, the conveyor belts and the boxes that wait to be stepped off.

extern Oscillatory oscillatory;
extern const uint8_t Mappings_MTZPlatformA[];

static void Reset(void) {
    memset(objects, 0, sizeof(Object) * 0x60);
    memset(f_switch, 0, sizeof(f_switch));
    memset(objstate, 0, sizeof(objstate));
    level_id = LEVEL_ID(ZoneId_MTZ, 0);
    scrpos_x.v = scrpos_y.v = 0;
    scrpos_x.f.u = 0xE00;
    camera_split = false;
    limit_btm2 = 0x720;
    debug_use = false;
    jpad2_press = 0;
    frame_count = 1;
    player->type = 1;
    player->routine = 2;
    player->y_rad = 19;
    player->pos.l.x.f.u = 0x1000;
    player->pos.l.y.f.u = 0x400;
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
#define SCRATCH_S16(obj, offset) ((obj)->scratch.s16[((offset) - 0x28) / 2])

// --- 67 ---

static void MTZ_TheTeleporterTakesACharacterAlongItsPath(void) {
    Reset();
    // path 1: two points, (0xBD8, 0x5F0) and (0xE00, 0x5F0)
    scrpos_x.f.u = 0xB00;
    Object *tele = Spawn(SLOT(0x20), 0x67, 1, 0xBD8, 0x5F0);
    Obj_MTZTeleport(tele);
    player->pos.l.x.f.u = 0xBD8;
    player->pos.l.y.f.u = 0x5F0 - 0x10;
    Obj_MTZTeleport(tele);
    CHECK_EQ(OBJ_CONTROL(player), 0x81);
    CHECK_EQ(player->pos.l.x.f.u, 0xBD8);
    CHECK_EQ(player->pos.l.y.f.u, 0x5F0);
    int frames = 0;
    while (OBJ_CONTROL(player) != 0 && frames < 2000) {
        Obj_MTZTeleport(tele);
        frames++;
    }
    CHECK(frames > 64 + 30 && frames < 64 + 45); // (the lift is 64 frames, the path $228 pixels at 16 a frame)
    CHECK_EQ(player->pos.l.x.f.u, 0xE00);
    CHECK_EQ(player->pos.l.y.f.u, 0x5F0);
    CHECK_EQ(player->xsp, 0);
    CHECK_EQ(player->ysp, 0);
}

static void MTZ_ANegativeSubtypeSendsHimBackwards(void) {
    Reset();
    Object *tele = Spawn(SLOT(0x20), 0x67, 0xFF, 0xE00, 0x5F0); // (path 1, the other way)
    Obj_MTZTeleport(tele);
    player->pos.l.x.f.u = 0xE00;
    player->pos.l.y.f.u = 0x5F0 - 0x10;
    Obj_MTZTeleport(tele);
    int frames = 0;
    while (OBJ_CONTROL(player) != 0 && frames < 2000) {
        Obj_MTZTeleport(tele);
        frames++;
    }
    CHECK_EQ(player->pos.l.x.f.u, 0xBD8);
    CHECK_EQ(player->pos.l.y.f.u, 0x5F0);
}

static void MTZ_ALongerPathFollowsEveryPoint(void) {
    Reset();
    // path 0: six points, from (0x728, 0x270) round to (0x728, 0x3F0)
    scrpos_x.f.u = 0x620;
    Object *tele = Spawn(SLOT(0x20), 0x67, 0x10, 0x728, 0x270); // (bit 4: he keeps his speed)
    Obj_MTZTeleport(tele);
    player->pos.l.x.f.u = 0x728;
    player->pos.l.y.f.u = 0x270 - 0x10;
    Obj_MTZTeleport(tele);
    int frames = 0;
    while (OBJ_CONTROL(player) != 0 && frames < 4000) {
        Obj_MTZTeleport(tele);
        frames++;
    }
    CHECK_EQ(player->pos.l.x.f.u, 0x728);
    CHECK_EQ(player->pos.l.y.f.u, 0x3F0);
    CHECK(player->ysp != 0 || player->xsp != 0); // (bit 4: still going)
}

static void MTZ_TheTeleporterIgnoresACharacterThatIsBusy(void) {
    Reset();
    scrpos_x.f.u = 0xB00;
    Object *tele = Spawn(SLOT(0x20), 0x67, 1, 0xBD8, 0x5F0);
    Obj_MTZTeleport(tele);
    player->pos.l.x.f.u = 0xBD8;
    player->pos.l.y.f.u = 0x5F0 - 0x10;
    OBJ_CONTROL(player) = 0x81; // (in a tube)
    Obj_MTZTeleport(tele);
    CHECK_EQ(tele->scratch.u8[0x2C - 0x28], 0);
    OBJ_CONTROL(player) = 0;
    player->pos.l.x.f.u = 0xBD8 + 0x20; // (and out of reach)
    Obj_MTZTeleport(tele);
    CHECK_EQ(tele->scratch.u8[0x2C - 0x28], 0);
}

// --- 6C ---

static void MTZ_ARingOfPlatformsIsMadeOfEight(void) {
    Reset();
    Object *spawner = Spawn(SLOT(0x20), 0x6C, 0x80, 0x1000, 0x300);
    Obj_MTZMovingPlatform(spawner);
    int count = 0;
    for (int i = 0; i < 0x60; i++)
        if (objects[i].type == 0x6C)
            count++;
    CHECK_EQ(count, 8);
    CHECK_EQ(spawner->scratch.u8[0], 1); // (itself the first)
    CHECK_EQ(spawner->pos.l.x.f.u, 0x1000);
    CHECK_EQ(spawner->pos.l.y.f.u, 0x300);
    bool found = false;
    for (int i = 0; i < 0x60; i++)
        if (&objects[i] != spawner && objects[i].type == 0x6C && objects[i].pos.l.x.f.u == 0x1000 - 0x20 && objects[i].pos.l.y.f.u == 0x300 + 0x3A)
            found = (objects[i].scratch.u8[0] == 3);
    CHECK(found);
}

static void MTZ_APlatformGoesRoundItsPathExactly(void) {
    Reset();
    Object *plat = Spawn(SLOT(0x20), 0x6C, 0x01, 0x1000, 0x300);
    SCRATCH_S16(plat, 0x30) = 0x1000;
    SCRATCH_S16(plat, 0x32) = 0x300;
    bool at_far_point = false;
    for (int i = 0; i < 3000; i++) {
        Obj_MTZMovingPlatform(plat);
        const int dx = plat->pos.l.x.f.u - 0x1000, dy = plat->pos.l.y.f.u - 0x300;
        CHECK(dx >= -0x20 && dx <= 0x20 && dy >= 0 && dy <= 0x100);
        if (dx == -0x20 && dy == 0xE0)
            at_far_point = true;
    }
    CHECK(at_far_point);
}

static void MTZ_AFlippedPlatformGoesTheOtherWayRound(void) {
    Reset();
    Object *plat = Spawn(SLOT(0x20), 0x6C, 0x00, 0x1000, 0x300);
    SCRATCH_S16(plat, 0x30) = 0x1000;
    SCRATCH_S16(plat, 0x32) = 0x300;
    plat->status.b = 1;
    Obj_MTZMovingPlatform(plat);
    // it starts at its first point and goes to the last: up and to the left... the point before the first is the tenth (22, 10)
    for (int i = 0; i < 40; i++)
        Obj_MTZMovingPlatform(plat);
    CHECK(plat->pos.l.x.f.u > 0x1000);
}

static void MTZ_APlatformCarriesWhoStandsOnIt(void) {
    Reset();
    Object *plat = Spawn(SLOT(0x20), 0x6C, 0x03, 0x1000 - 0x20, 0x300 + 0x3A);
    SCRATCH_S16(plat, 0x30) = 0x1000;
    SCRATCH_S16(plat, 0x32) = 0x300;
    Obj_MTZMovingPlatform(plat);
    player->pos.l.x.f.u = plat->pos.l.x.f.u;
    player->pos.l.y.f.u = (int16_t)(plat->pos.l.y.f.u - 8 - player->y_rad);
    player->status.p.f.in_air = false;
    player->status.p.f.object_stand = true;
    plat->status.b |= 8;
    const int16_t y0 = player->pos.l.y.f.u;
    for (int i = 0; i < 20; i++)
        Obj_MTZMovingPlatform(plat);
    CHECK(player->pos.l.y.f.u > y0); // (down with it)
    CHECK_EQ(player->pos.l.y.f.u + player->y_rad + 8, plat->pos.l.y.f.u);
}

// --- 6E ---

static void MTZ_TheMachineFollowsTheOscillators(void) {
    Reset();
    Object *m = Spawn(SLOT(0x20), 0x6E, 0x00, 0x1000, 0x300);
    oscillatory.state[8][0] = 0x3800;
    oscillatory.state[9][0] = 0x3800;
    Obj_MTZMachine(m);
    Obj_MTZMachine(m);
    CHECK_EQ(m->pos.l.x.f.u, 0x1000);
    CHECK_EQ(m->pos.l.y.f.u, 0x300);
    oscillatory.state[8][0] = 0x4800;
    oscillatory.state[9][0] = 0x2800;
    Obj_MTZMachine(m);
    CHECK_EQ(m->pos.l.x.f.u, 0x1010);
    CHECK_EQ(m->pos.l.y.f.u, 0x300 - 0x10);
}

static void MTZ_AFlippedMachineGoesTheOtherWay(void) {
    Reset();
    Object *m = Spawn(SLOT(0x20), 0x6E, 0x01, 0x1000, 0x300);
    oscillatory.state[8][0] = 0x4800;
    oscillatory.state[9][0] = 0x2800;
    Obj_MTZMachine(m);
    Obj_MTZMachine(m);
    CHECK_EQ(m->pos.l.x.f.u, 0x1000 - 0x10);
    CHECK_EQ(m->pos.l.y.f.u, 0x300 + 0x10);
}

static void MTZ_AMirroredMachineSwapsItsAxes(void) {
    Reset();
    Object *m = Spawn(SLOT(0x20), 0x6E, 0x02, 0x1000, 0x300);
    oscillatory.state[8][0] = 0x4800; // +$10
    oscillatory.state[9][0] = 0x3800; // 0
    Obj_MTZMachine(m);
    Obj_MTZMachine(m);
    CHECK_EQ(m->pos.l.x.f.u, 0x1000); // (x: the other's, negated... zero)
    CHECK_EQ(m->pos.l.y.f.u, 0x300 - 0x10);
}

static void MTZ_ThePartOfAMachineIsNotSolid(void) {
    Reset();
    Object *m = Spawn(SLOT(0x20), 0x6E, 0x30, 0x1000, 0x300);
    Obj_MTZMachine(m);
    CHECK_EQ(m->routine, 4);
    CHECK_EQ(m->frame, 3);
}

// --- 6F ---

static void MTZ_TheElevatorWaitsToBeStoodOnAndThenGoesForEver(void) {
    Reset();
    Object *e = Spawn(SLOT(0x20), 0x6F, 0x01, 0x1000, 0x300);
    Obj_MTZElevator(e);
    const int16_t x0 = e->pos.l.x.f.u, y0 = e->pos.l.y.f.u;
    for (int i = 0; i < 20; i++)
        Obj_MTZElevator(e);
    CHECK_EQ(e->pos.l.x.f.u, x0); // (still)
    e->status.b |= 8;
    Obj_MTZElevator(e);
    CHECK_EQ(e->scratch.u8[0], 2);
    for (int i = 0; i < 0x100; i++)
        Obj_MTZElevator(e);
    CHECK_EQ(e->pos.l.x.f.u, x0 + 0x100);
    CHECK_EQ(e->pos.l.y.f.u, y0 - 0x80);
}

static void MTZ_AnElevatorOfModeZeroNeverMoves(void) {
    Reset();
    Object *e = Spawn(SLOT(0x20), 0x6F, 0x00, 0x1000, 0x300);
    e->status.b |= 8;
    for (int i = 0; i < 50; i++)
        Obj_MTZElevator(e);
    CHECK_EQ(e->pos.l.x.f.u, 0x1000);
}

// (the middle of the elevator's table: its top $20 above its centre, $40 thick)
static const int8_t elevator_middle[256] = { [128] = 0x20, [129] = 0x40 };

static void MTZ_TheElevatorTopIsASlopeOfItsOwn(void) {
    Reset();
    Object *e = Spawn(SLOT(0x20), 0x6F, 0x00, 0x1000, 0x300);
    Obj_MTZElevator(e); // (init)
    player->pos.l.x.f.u = 0x1000;
    player->pos.l.y.f.u = (int16_t)(0x300 - 0x20 - player->y_rad + 2);
    player->status.p.f.in_air = true;
    player->ysp = 0x100;
    const int32_t r = Solid_CharacterDouble(e, player, SolidChar_Sonic, 0x80, 0x1000, elevator_middle);
    CHECK_EQ(r, -1);
    CHECK(player->status.p.f.object_stand);
}

// --- 6A ---

static void MTZ_ABoxWaitsUntilWhoStoodOnItStepsOff(void) {
    Reset();
    Object *b = Spawn(SLOT(0x20), 0x6A, 0x00, 0x1000, 0x300);
    Obj_RotatingBoxes(b);
    for (int i = 0; i < 20; i++)
        Obj_RotatingBoxes(b);
    CHECK_EQ(b->pos.l.y.f.u, 0x300);
    b->status.b |= 8; // (stood on)
    for (int i = 0; i < 20; i++)
        Obj_RotatingBoxes(b);
    CHECK_EQ(b->pos.l.y.f.u, 0x300); // (still, while he is on it)
    b->status.b &= (uint8_t)~8; // (steps off)
    for (int i = 0; i < 17; i++) // (the first frame only sees him go)
        Obj_RotatingBoxes(b);
    CHECK_EQ(b->pos.l.y.f.u, 0x300 + 0x40); // (down 4 a frame for 16)
    for (int i = 0; i < 20; i++)
        Obj_RotatingBoxes(b);
    CHECK_EQ(b->pos.l.y.f.u, 0x300 + 0x40); // (and it waits again)
}

// --- 70 ---

static void MTZ_AGearIsEightGears(void) {
    Reset();
    Object *g = Spawn(SLOT(0x20), 0x70, 0, 0x1000, 0x300);
    Obj_MTZGear(g);
    int count = 0;
    for (int i = 0; i < 0x60; i++)
        if (objects[i].type == 0x70)
            count++;
    CHECK_EQ(count, 8);
    CHECK_EQ(g->pos.l.x.f.u, 0x1000);
    CHECK_EQ(g->pos.l.y.f.u, 0x300 - 0x48);
}

static void MTZ_TheGearsMoveOnEverySixteenthFrame(void) {
    Reset();
    Object *g = Spawn(SLOT(0x20), 0x70, 0, 0x1000, 0x300);
    Obj_MTZGear(g);
    const int16_t y0 = g->pos.l.y.f.u, x0 = g->pos.l.x.f.u;
    frame_count = 5;
    Obj_MTZGear(g);
    CHECK_EQ(g->pos.l.y.f.u, y0);
    CHECK_EQ(g->pos.l.x.f.u, x0);
    frame_count = 16;
    Obj_MTZGear(g);
    CHECK(g->pos.l.y.f.u != y0 || g->pos.l.x.f.u != x0);
}

static void MTZ_TheGearsGoAllTheWayRoundAndBack(void) {
    Reset();
    Object *g = Spawn(SLOT(0x20), 0x70, 0, 0x1000, 0x300);
    Obj_MTZGear(g);
    const int16_t y0 = g->pos.l.y.f.u, x0 = g->pos.l.x.f.u;
    for (int i = 0; i < 32; i++) { // (four rows of the ring for each of the eight places: a whole turn is 32 steps)
        frame_count = 16 * (i + 1);
        Obj_MTZGear(g);
    }
    CHECK_EQ(g->pos.l.x.f.u, x0);
    CHECK_EQ(g->pos.l.y.f.u, y0);
}

// --- 72 ---

static void MTZ_TheConveyorMovesWhoIsOnTheGroundOnIt(void) {
    Reset();
    Object *c = Spawn(SLOT(0x20), 0x72, 0x02, 0x1000, 0x300);
    player->pos.l.x.f.u = 0x1000;
    player->pos.l.y.f.u = 0x300 - 0x10;
    player->status.p.f.in_air = false;
    Obj_MTZConveyor(c);
    CHECK_EQ(player->pos.l.x.f.u, 0x1002);
    Obj_MTZConveyor(c);
    CHECK_EQ(player->pos.l.x.f.u, 0x1004);
}

static void MTZ_TheConveyorLeavesWhoIsInTheAirAlone(void) {
    Reset();
    Object *c = Spawn(SLOT(0x20), 0x72, 0x02, 0x1000, 0x300);
    player->pos.l.x.f.u = 0x1000;
    player->pos.l.y.f.u = 0x300 - 0x10;
    player->status.p.f.in_air = true;
    Obj_MTZConveyor(c);
    CHECK_EQ(player->pos.l.x.f.u, 0x1000);
}

static void MTZ_AFlippedConveyorGoesLeftAndReachesSubtypeTimesSixteenEachSide(void) {
    Reset();
    Object *c = Spawn(SLOT(0x20), 0x72, 0x02, 0x1000, 0x300);
    c->status.b = 1;
    player->pos.l.x.f.u = 0x1000 + 0x1F;
    player->pos.l.y.f.u = 0x300 - 0x10;
    Obj_MTZConveyor(c);
    CHECK_EQ(player->pos.l.x.f.u, 0x1000 + 0x1D);
    player->pos.l.x.f.u = 0x1000 + 0x20; // (just out of it: the strip is $20 either side, and the right edge itself is out)
    Obj_MTZConveyor(c);
    CHECK_EQ(player->pos.l.x.f.u, 0x1000 + 0x20);
    player->pos.l.x.f.u = 0x1000 - 0x20; // (the left edge is in)
    Obj_MTZConveyor(c);
    CHECK_EQ(player->pos.l.x.f.u, 0x1000 - 0x22);
}

// --- 6B ---

static void MTZ_TheBlockOfMetropolisIsTheSlidePlatformsOwn(void) {
    Reset();
    Object *b = Spawn(SLOT(0x20), 0x6B, 0x00, 0x1000, 0x300);
    Obj_CPZBlock(b);
    CHECK(b->mappings == Mappings_MTZPlatformA);
    CHECK_EQ(b->width_pixels, 0x20);
}

void RegisterMTZObjectTests(void) {
    RUN_TEST(MTZ_TheTeleporterTakesACharacterAlongItsPath);
    RUN_TEST(MTZ_ANegativeSubtypeSendsHimBackwards);
    RUN_TEST(MTZ_ALongerPathFollowsEveryPoint);
    RUN_TEST(MTZ_TheTeleporterIgnoresACharacterThatIsBusy);
    RUN_TEST(MTZ_ARingOfPlatformsIsMadeOfEight);
    RUN_TEST(MTZ_APlatformGoesRoundItsPathExactly);
    RUN_TEST(MTZ_AFlippedPlatformGoesTheOtherWayRound);
    RUN_TEST(MTZ_APlatformCarriesWhoStandsOnIt);
    RUN_TEST(MTZ_TheMachineFollowsTheOscillators);
    RUN_TEST(MTZ_AFlippedMachineGoesTheOtherWay);
    RUN_TEST(MTZ_AMirroredMachineSwapsItsAxes);
    RUN_TEST(MTZ_ThePartOfAMachineIsNotSolid);
    RUN_TEST(MTZ_TheElevatorWaitsToBeStoodOnAndThenGoesForEver);
    RUN_TEST(MTZ_AnElevatorOfModeZeroNeverMoves);
    RUN_TEST(MTZ_TheElevatorTopIsASlopeOfItsOwn);
    RUN_TEST(MTZ_ABoxWaitsUntilWhoStoodOnItStepsOff);
    RUN_TEST(MTZ_AGearIsEightGears);
    RUN_TEST(MTZ_TheGearsMoveOnEverySixteenthFrame);
    RUN_TEST(MTZ_TheGearsGoAllTheWayRoundAndBack);
    RUN_TEST(MTZ_TheConveyorMovesWhoIsOnTheGroundOnIt);
    RUN_TEST(MTZ_TheConveyorLeavesWhoIsInTheAirAlone);
    RUN_TEST(MTZ_AFlippedConveyorGoesLeftAndReachesSubtypeTimesSixteenEachSide);
    RUN_TEST(MTZ_TheBlockOfMetropolisIsTheSlidePlatformsOwn);
}
