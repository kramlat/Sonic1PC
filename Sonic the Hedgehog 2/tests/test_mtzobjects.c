#include "test.h"

#include <string.h>

#include "Game.h"
#include "Level.h"
#include "Object.h"
#include "SstLoad.h"
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

// (the slots below $20 are taken, so that the objects a test makes get the slots the old tests named)
static void SstTestFiller(Sst *self) { (void)self; }

static void Reset(void) {
    Sst_Reset();
    sst_load_everything_in_range = true;
    for (int i = 4; i < 0x20; i++)
        sst[i].code = SstTestFiller;
    memset(f_switch, 0, sizeof(f_switch));
    memset(sst_respawn_table, 0, sizeof(sst_respawn_table));
    level_id = LEVEL_ID(ZoneId_MTZ, 0);
    scrpos_x.v = scrpos_y.v = 0;
    scrpos_x.f.u = 0xE00;
    camera_split = false;
    limit_btm2 = 0x720;
    debug_use = false;
    jpad2_press = 0;
    frame_count = 1;
    player->code = sst_object_codes[1];
    player->routine = 2;
    player->y_radius = 19;
    player->x_pos.f.u = 0x1000;
    player->y_pos.f.u = 0x400;
    player->status = 0;
    OBJ_CONTROL(player) = 0;
    {
        static const SstObjectIndex index_ = {sst_object_codes, 0x100};
        SstLoadCamera camera = {(int16_t)scrpos_x.f.u, (int16_t)scrpos_y.f.u, false, 0xFFF};
        SstLoad_Init(&index_, NULL, false, &camera);
    }
}

static Sst *Spawn(Sst *slot, uint8_t type, uint8_t subtype, int16_t x, int16_t y) {
    memset(slot, 0, sizeof(Sst));
    slot->code = sst_object_codes[type];
    slot->subtype = subtype;
    slot->x_pos.f.u = x;
    slot->y_pos.f.u = y;
    return slot;
}

#define SLOT(n) (&sst[n])
#define SCRATCH_S16(obj, offset) ((obj)->own.s16[((offset) - 0x28) / 2])

// --- 67 ---

static void MTZ_TheTeleporterTakesACharacterAlongItsPath(void) {
    Reset();
    // path 1 (the alpha's): two points, (0xC58, 0x5F0) and (0xE28, 0x5F0)
    scrpos_x.f.u = 0xB80;
    Sst *tele = Spawn(SLOT(0x20), 0x67, 1, 0xC58, 0x5F0);
    Obj_MTZTeleport(tele);
    player->x_pos.f.u = 0xC58;
    player->y_pos.f.u = 0x5F0 - 0x10;
    Obj_MTZTeleport(tele);
    CHECK_EQ(OBJ_CONTROL(player), 0x81);
    CHECK_EQ(player->x_pos.f.u, 0xC58);
    CHECK_EQ(player->y_pos.f.u, 0x5F0);
    int frames = 0;
    while (OBJ_CONTROL(player) != 0 && frames < 2000) {
        Obj_MTZTeleport(tele);
        frames++;
    }
    CHECK(frames > 64 + 20 && frames < 64 + 40); // (the lift is 64 frames, the path $1D0 pixels at 16 a frame)
    CHECK_EQ(player->x_pos.f.u, 0xE28);
    CHECK_EQ(player->y_pos.f.u, 0x5F0);
    CHECK_EQ(player->x_vel, 0);
    CHECK_EQ(player->y_vel, 0);
}

static void MTZ_ANegativeSubtypeSendsHimBackwards(void) {
    Reset();
    Sst *tele = Spawn(SLOT(0x20), 0x67, 0xFF, 0xE28, 0x5F0); // (path 1, the other way)
    Obj_MTZTeleport(tele);
    player->x_pos.f.u = 0xE28;
    player->y_pos.f.u = 0x5F0 - 0x10;
    Obj_MTZTeleport(tele);
    int frames = 0;
    while (OBJ_CONTROL(player) != 0 && frames < 2000) {
        Obj_MTZTeleport(tele);
        frames++;
    }
    CHECK_EQ(player->x_pos.f.u, 0xC58);
    CHECK_EQ(player->y_pos.f.u, 0x5F0);
}

static void MTZ_ALongerPathFollowsEveryPoint(void) {
    Reset();
    // path 0 (the alpha's): six points, from (0x7A8, 0x270) round to (0x7A8, 0x3F0)
    scrpos_x.f.u = 0x6A0;
    Sst *tele = Spawn(SLOT(0x20), 0x67, 0x10, 0x7A8, 0x270); // (bit 4: he keeps his speed)
    Obj_MTZTeleport(tele);
    player->x_pos.f.u = 0x7A8;
    player->y_pos.f.u = 0x270 - 0x10;
    Obj_MTZTeleport(tele);
    int frames = 0;
    while (OBJ_CONTROL(player) != 0 && frames < 4000) {
        Obj_MTZTeleport(tele);
        frames++;
    }
    CHECK_EQ(player->x_pos.f.u, 0x7A8);
    CHECK_EQ(player->y_pos.f.u, 0x3F0);
    CHECK(player->y_vel != 0 || player->x_vel != 0); // (bit 4: still going)
}

static void MTZ_TheTeleporterIgnoresACharacterThatIsBusy(void) {
    Reset();
    scrpos_x.f.u = 0xB80;
    Sst *tele = Spawn(SLOT(0x20), 0x67, 1, 0xC58, 0x5F0);
    Obj_MTZTeleport(tele);
    player->x_pos.f.u = 0xC58;
    player->y_pos.f.u = 0x5F0 - 0x10;
    OBJ_CONTROL(player) = 0x81; // (in a tube)
    Obj_MTZTeleport(tele);
    CHECK_EQ(tele->own.u8[0x2C - 0x28], 0);
    OBJ_CONTROL(player) = 0;
    player->x_pos.f.u = 0xC58 + 0x20; // (and out of reach)
    Obj_MTZTeleport(tele);
    CHECK_EQ(tele->own.u8[0x2C - 0x28], 0);
}

// --- 6C ---

static void MTZ_ARingOfPlatformsIsMadeOfEight(void) {
    Reset();
    Sst *spawner = Spawn(SLOT(0x20), 0x6C, 0x80, 0x1000, 0x300);
    Obj_MTZMovingPlatform(spawner);
    int count = 0;
    for (int i = 0; i < 0x60; i++)
        if (sst[i].code == sst_object_codes[0x6C])
            count++;
    CHECK_EQ(count, 8);
    CHECK_EQ(spawner->subtype, 1); // (itself the first)
    CHECK_EQ(spawner->x_pos.f.u, 0x1000);
    CHECK_EQ(spawner->y_pos.f.u, 0x300);
    bool found = false;
    for (int i = 0; i < 0x60; i++)
        if (&sst[i] != spawner && sst[i].code == sst_object_codes[0x6C] && sst[i].x_pos.f.u == 0x1000 - 0x20 && sst[i].y_pos.f.u == 0x300 + 0x3A)
            found = (sst[i].subtype == 3);
    CHECK(found);
}

static void MTZ_APlatformGoesRoundItsPathExactly(void) {
    Reset();
    Sst *plat = Spawn(SLOT(0x20), 0x6C, 0x01, 0x1000, 0x300);
    SCRATCH_S16(plat, 0x30) = 0x1000;
    SCRATCH_S16(plat, 0x32) = 0x300;
    bool at_far_point = false;
    for (int i = 0; i < 3000; i++) {
        Obj_MTZMovingPlatform(plat);
        const int dx = plat->x_pos.f.u - 0x1000, dy = plat->y_pos.f.u - 0x300;
        CHECK(dx >= -0x20 && dx <= 0x20 && dy >= 0 && dy <= 0x100);
        if (dx == -0x20 && dy == 0xE0)
            at_far_point = true;
    }
    CHECK(at_far_point);
}

static void MTZ_AFlippedPlatformGoesTheOtherWayRound(void) {
    Reset();
    Sst *plat = Spawn(SLOT(0x20), 0x6C, 0x00, 0x1000, 0x300);
    SCRATCH_S16(plat, 0x30) = 0x1000;
    SCRATCH_S16(plat, 0x32) = 0x300;
    plat->status = 1;
    Obj_MTZMovingPlatform(plat);
    // it starts at its first point and goes to the last: up and to the left... the point before the first is the tenth (22, 10)
    for (int i = 0; i < 40; i++)
        Obj_MTZMovingPlatform(plat);
    CHECK(plat->x_pos.f.u > 0x1000);
}

static void MTZ_APlatformCarriesWhoStandsOnIt(void) {
    Reset();
    Sst *plat = Spawn(SLOT(0x20), 0x6C, 0x03, 0x1000 - 0x20, 0x300 + 0x3A);
    SCRATCH_S16(plat, 0x30) = 0x1000;
    SCRATCH_S16(plat, 0x32) = 0x300;
    Obj_MTZMovingPlatform(plat);
    player->x_pos.f.u = plat->x_pos.f.u;
    player->y_pos.f.u = (int16_t)(plat->y_pos.f.u - 8 - player->y_radius);
    player->status &= (uint8_t)~SST_PSTATUS_IN_AIR;
    player->status |= SST_PSTATUS_OBJECT_STAND;
    plat->status |= 8;
    const int16_t y0 = player->y_pos.f.u;
    for (int i = 0; i < 20; i++)
        Obj_MTZMovingPlatform(plat);
    CHECK(player->y_pos.f.u > y0); // (down with it)
    CHECK_EQ(player->y_pos.f.u + player->y_radius + 8, plat->y_pos.f.u);
}

// --- 6E ---

static void MTZ_TheMachineFollowsTheOscillators(void) {
    Reset();
    Sst *m = Spawn(SLOT(0x20), 0x6E, 0x00, 0x1000, 0x300);
    oscillatory.state[8][0] = 0x3800;
    oscillatory.state[9][0] = 0x3800;
    Obj_MTZMachine(m);
    Obj_MTZMachine(m);
    CHECK_EQ(m->x_pos.f.u, 0x1000);
    CHECK_EQ(m->y_pos.f.u, 0x300);
    oscillatory.state[8][0] = 0x4800;
    oscillatory.state[9][0] = 0x2800;
    Obj_MTZMachine(m);
    CHECK_EQ(m->x_pos.f.u, 0x1010);
    CHECK_EQ(m->y_pos.f.u, 0x300 - 0x10);
}

static void MTZ_AFlippedMachineGoesTheOtherWay(void) {
    Reset();
    Sst *m = Spawn(SLOT(0x20), 0x6E, 0x01, 0x1000, 0x300);
    oscillatory.state[8][0] = 0x4800;
    oscillatory.state[9][0] = 0x2800;
    Obj_MTZMachine(m);
    Obj_MTZMachine(m);
    CHECK_EQ(m->x_pos.f.u, 0x1000 - 0x10);
    CHECK_EQ(m->y_pos.f.u, 0x300 + 0x10);
}

static void MTZ_AMirroredMachineSwapsItsAxes(void) {
    Reset();
    Sst *m = Spawn(SLOT(0x20), 0x6E, 0x02, 0x1000, 0x300);
    oscillatory.state[8][0] = 0x4800; // +$10
    oscillatory.state[9][0] = 0x3800; // 0
    Obj_MTZMachine(m);
    Obj_MTZMachine(m);
    CHECK_EQ(m->x_pos.f.u, 0x1000); // (x: the other's, negated... zero)
    CHECK_EQ(m->y_pos.f.u, 0x300 - 0x10);
}

static void MTZ_ThePartOfAMachineIsNotSolid(void) {
    Reset();
    Sst *m = Spawn(SLOT(0x20), 0x6E, 0x30, 0x1000, 0x300);
    Obj_MTZMachine(m);
    CHECK_EQ(m->routine, 4);
    CHECK_EQ(m->mapping_frame, 3);
}

// --- 6F ---

static void MTZ_TheElevatorWaitsToBeStoodOnAndThenGoesForEver(void) {
    Reset();
    Sst *e = Spawn(SLOT(0x20), 0x6F, 0x01, 0x1000, 0x300);
    Obj_MTZElevator(e);
    const int16_t x0 = e->x_pos.f.u, y0 = e->y_pos.f.u;
    for (int i = 0; i < 20; i++)
        Obj_MTZElevator(e);
    CHECK_EQ(e->x_pos.f.u, x0); // (still)
    e->status |= 8;
    Obj_MTZElevator(e);
    CHECK_EQ(e->subtype, 2);
    for (int i = 0; i < 0x100; i++)
        Obj_MTZElevator(e);
    CHECK_EQ(e->x_pos.f.u, x0 + 0x100);
    CHECK_EQ(e->y_pos.f.u, y0 - 0x80);
}

static void MTZ_AnElevatorOfModeZeroNeverMoves(void) {
    Reset();
    Sst *e = Spawn(SLOT(0x20), 0x6F, 0x00, 0x1000, 0x300);
    e->status |= 8;
    for (int i = 0; i < 50; i++)
        Obj_MTZElevator(e);
    CHECK_EQ(e->x_pos.f.u, 0x1000);
}

// (the middle of the elevator's table: its top $20 above its centre, $40 thick)
static const int8_t elevator_middle[256] = { [128] = 0x20, [129] = 0x40 };

static void MTZ_TheElevatorTopIsASlopeOfItsOwn(void) {
    Reset();
    Sst *e = Spawn(SLOT(0x20), 0x6F, 0x00, 0x1000, 0x300);
    Obj_MTZElevator(e); // (init)
    player->x_pos.f.u = 0x1000;
    player->y_pos.f.u = (int16_t)(0x300 - 0x20 - player->y_radius + 2);
    player->status |= SST_PSTATUS_IN_AIR;
    player->y_vel = 0x100;
    const int32_t r = Solid_CharacterDouble(e, player, SolidChar_Sonic, 0x80, 0x1000, elevator_middle);
    CHECK_EQ(r, -1);
    CHECK(((player->status & SST_PSTATUS_OBJECT_STAND) != 0));
}

// --- 6A ---

static void MTZ_ABoxWaitsUntilWhoStoodOnItStepsOff(void) {
    Reset();
    Sst *b = Spawn(SLOT(0x20), 0x6A, 0x00, 0x1000, 0x300);
    Obj_RotatingBoxes(b);
    for (int i = 0; i < 20; i++)
        Obj_RotatingBoxes(b);
    CHECK_EQ(b->y_pos.f.u, 0x300);
    b->status |= 8; // (stood on)
    for (int i = 0; i < 20; i++)
        Obj_RotatingBoxes(b);
    CHECK_EQ(b->y_pos.f.u, 0x300); // (still, while he is on it)
    b->status &= (uint8_t)~8; // (steps off)
    for (int i = 0; i < 17; i++) // (the first frame only sees him go)
        Obj_RotatingBoxes(b);
    CHECK_EQ(b->y_pos.f.u, 0x300 + 0x40); // (down 4 a frame for 16)
    for (int i = 0; i < 20; i++)
        Obj_RotatingBoxes(b);
    CHECK_EQ(b->y_pos.f.u, 0x300 + 0x40); // (and it waits again)
}

// --- 70 ---

static void MTZ_AGearIsEightGears(void) {
    Reset();
    Sst *g = Spawn(SLOT(0x20), 0x70, 0, 0x1000, 0x300);
    Obj_MTZGear(g);
    int count = 0;
    for (int i = 0; i < 0x60; i++)
        if (sst[i].code == sst_object_codes[0x70])
            count++;
    CHECK_EQ(count, 8);
    CHECK_EQ(g->x_pos.f.u, 0x1000);
    CHECK_EQ(g->y_pos.f.u, 0x300 - 0x48);
}

static void MTZ_TheGearsMoveOnEverySixteenthFrame(void) {
    Reset();
    Sst *g = Spawn(SLOT(0x20), 0x70, 0, 0x1000, 0x300);
    Obj_MTZGear(g);
    const int16_t y0 = g->y_pos.f.u, x0 = g->x_pos.f.u;
    frame_count = 5;
    Obj_MTZGear(g);
    CHECK_EQ(g->y_pos.f.u, y0);
    CHECK_EQ(g->x_pos.f.u, x0);
    frame_count = 16;
    Obj_MTZGear(g);
    CHECK(g->y_pos.f.u != y0 || g->x_pos.f.u != x0);
}

static void MTZ_TheGearsGoAllTheWayRoundAndBack(void) {
    Reset();
    Sst *g = Spawn(SLOT(0x20), 0x70, 0, 0x1000, 0x300);
    Obj_MTZGear(g);
    const int16_t y0 = g->y_pos.f.u, x0 = g->x_pos.f.u;
    for (int i = 0; i < 32; i++) { // (four rows of the ring for each of the eight places: a whole turn is 32 steps)
        frame_count = 16 * (i + 1);
        Obj_MTZGear(g);
    }
    CHECK_EQ(g->x_pos.f.u, x0);
    CHECK_EQ(g->y_pos.f.u, y0);
}

// --- 72 ---

static void MTZ_TheConveyorMovesWhoIsOnTheGroundOnIt(void) {
    Reset();
    Sst *c = Spawn(SLOT(0x20), 0x72, 0x02, 0x1000, 0x300);
    player->x_pos.f.u = 0x1000;
    player->y_pos.f.u = 0x300 - 0x10;
    player->status &= (uint8_t)~SST_PSTATUS_IN_AIR;
    Obj_MTZConveyor(c);
    CHECK_EQ(player->x_pos.f.u, 0x1002);
    Obj_MTZConveyor(c);
    CHECK_EQ(player->x_pos.f.u, 0x1004);
}

static void MTZ_TheConveyorLeavesWhoIsInTheAirAlone(void) {
    Reset();
    Sst *c = Spawn(SLOT(0x20), 0x72, 0x02, 0x1000, 0x300);
    player->x_pos.f.u = 0x1000;
    player->y_pos.f.u = 0x300 - 0x10;
    player->status |= SST_PSTATUS_IN_AIR;
    Obj_MTZConveyor(c);
    CHECK_EQ(player->x_pos.f.u, 0x1000);
}

static void MTZ_AFlippedConveyorGoesLeftAndReachesSubtypeTimesSixteenEachSide(void) {
    Reset();
    Sst *c = Spawn(SLOT(0x20), 0x72, 0x02, 0x1000, 0x300);
    c->status = 1;
    player->x_pos.f.u = 0x1000 + 0x1F;
    player->y_pos.f.u = 0x300 - 0x10;
    Obj_MTZConveyor(c);
    CHECK_EQ(player->x_pos.f.u, 0x1000 + 0x1D);
    player->x_pos.f.u = 0x1000 + 0x20; // (just out of it: the strip is $20 either side, and the right edge itself is out)
    Obj_MTZConveyor(c);
    CHECK_EQ(player->x_pos.f.u, 0x1000 + 0x20);
    player->x_pos.f.u = 0x1000 - 0x20; // (the left edge is in)
    Obj_MTZConveyor(c);
    CHECK_EQ(player->x_pos.f.u, 0x1000 - 0x22);
}

// --- 6B ---

static void MTZ_TheBlockOfMetropolisIsTheSlidePlatformsOwn(void) {
    Reset();
    Sst *b = Spawn(SLOT(0x20), 0x6B, 0x00, 0x1000, 0x300);
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
