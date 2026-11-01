#include "test.h"

#include <string.h>

#include "Level.h"
#include "Object.h"
#include "SstLoad.h"
#include "Object/CPZBadniks.h"
#include "Object/CPZObjects.h"
#include "Object/CPZBoss.h"
#include "Object/SFZObjects.h"
#include "Object/SkyBadniks.h"
#include "Object/Explosion.h"
#include "Object/Coconuts.h"
#include "Object/Sonic.h"
#include "Object/CharControl.h"
#include "Game.h"

// Chemical Plant's objects: the one way barrier (2D), the speed booster (1B) and the staircase blocks (6B), as the prototype's routines have them.

// (the slots below $20 are taken, so that the objects a test makes get the slots the old tests named)
static void SstTestFiller(Sst *self) { (void)self; }

static void Reset(void) {
    Sst_Reset();
    sst_load_everything_in_range = true;
    for (int i = 4; i < 0x20; i++)
        sst[i].code = SstTestFiller;
    level_id = LEVEL_ID(ZoneId_CPZ, 0);
    scrpos_x.v = scrpos_y.v = 0x100 << 16; // (near the objects below: they are not forgotten for being far away)
    limit_btm1 = 0x720;
    player->routine = 2;
    player->x_pos.f.u = 0x1000; // (far from everything, until a test puts him somewhere)
    player->y_pos.f.u = 0x1000;
    {
        static const SstObjectIndex index_ = {sst_object_codes, 0x100};
        SstLoadCamera camera = {(int16_t)scrpos_x.f.u, (int16_t)scrpos_y.f.u, false, 0xFFF};
        SstLoad_Init(&index_, NULL, false, &camera);
    }
}

static Sst *Spawn(uint8_t type, uint8_t subtype, int16_t x, int16_t y) {
    SstLoad_Seek((int16_t)(x - 0x100)); // (the loader keeps what is near the camera: the camera is where the object is)
    Sst *obj = Sst_Spawn(type, x, y);
    obj->subtype = subtype;
    return obj;
}

// The barrier: it rises 8 a frame (to $40) while a character is in the strip $200 wide on its open side, and sinks again when nobody is
// (a barrier is at least $200 from the left of the level, as the strip's left edge is an unsigned x)
static void CPZObjects_BarrierRisesWhileSonicIsNearAndSinksAfter(void) {
    Reset();
    Sst *obj = Spawn(0x2D, 0, 800, 300);
    Obj_CPZBarrier(obj);
    CHECK_EQ(obj->y_pos.f.u, 300);
    player->x_pos.f.u = 700; // inside [x - $200, x)
    player->y_pos.f.u = 300; // (and within $20 of the barrier's base: the pre-beta's strip is a box)
    for (int i = 0; i < 4; i++)
        Obj_CPZBarrier(obj);
    CHECK_EQ(obj->y_pos.f.u, 300 - 4 * 8);
    for (int i = 0; i < 20; i++)
        Obj_CPZBarrier(obj);
    CHECK_EQ(obj->y_pos.f.u, 300 - 0x40); // (and no higher)
    player->x_pos.f.u = 0x1000; // gone
    for (int i = 0; i < 4; i++)
        Obj_CPZBarrier(obj);
    CHECK_EQ(obj->y_pos.f.u, 300 - 0x40 + 4 * 8);
    for (int i = 0; i < 20; i++)
        Obj_CPZBarrier(obj);
    CHECK_EQ(obj->y_pos.f.u, 300);
}

static void CPZObjects_BarrierDoesNotOpenForTheFarSide(void) {
    Reset();
    Sst *obj = Spawn(0x2D, 0, 800, 300);
    Obj_CPZBarrier(obj);
    player->x_pos.f.u = 900; // beyond it, the side it shuts
    for (int i = 0; i < 10; i++)
        Obj_CPZBarrier(obj);
    CHECK_EQ(obj->y_pos.f.u, 300);
}

static void CPZObjects_ABoosterGivesTheSpeedItsSubtypeSays(void) {
    Reset();
    Sst *obj = Spawn(0x1B, 0, 200, 300);
    player->x_pos.f.u = 200;
    player->y_pos.f.u = 300;
    player->status &= (uint8_t)~SST_PSTATUS_IN_AIR;
    Obj_CPZBooster(obj);
    CHECK_EQ(player->x_vel, 0x1000);
    CHECK_EQ(player->ground_vel, 0x1000);
    CHECK_EQ(((Scratch_Sonic *)player->own.u8)->control_lock, 0xF);

    Sst *slow = Spawn(0x1B, 2, 200, 300); // bit 1: the slower one
    player->x_vel = 0;
    Obj_CPZBooster(slow);
    CHECK_EQ(player->x_vel, 0xA00);

    Sst *back = Spawn(0x1B, 0, 200, 300);
    player->x_vel = 0;
    Obj_CPZBooster(back); // (its first call sets it up, and gives the speed)
    back->status |= SST_STATUS_X_FLIP;
    player->x_vel = 0;
    Obj_CPZBooster(back);
    CHECK_EQ(player->x_vel, -0x1000);
    CHECK(((player->status & SST_PSTATUS_X_FLIP) != 0));

    player->x_vel = 0x1200; // (the alpha's: a character already that fast is left as he is)
    Obj_CPZBooster(slow);
    CHECK_EQ(player->x_vel, 0x1200);
}

static void CPZObjects_ABoosterLeavesWhoIsInTheAirAlone(void) {
    Reset();
    Sst *obj = Spawn(0x1B, 0, 200, 300);
    player->x_pos.f.u = 200;
    player->y_pos.f.u = 300;
    player->status |= SST_PSTATUS_IN_AIR;
    player->x_vel = 0x123;
    Obj_CPZBooster(obj);
    CHECK_EQ(player->x_vel, 0x123);
}

// The staircase block of kind 5 waits to be stood on, and then (kind 6) falls
static void CPZObjects_ABlockFallsOnceItIsStoodOn(void) {
    Reset();
    Sst *obj = Spawn(0x6B, 5, 300, 300);
    Obj_CPZBlock(obj);
    const int16_t y0 = obj->y_pos.f.u;
    obj->status |= 1 << 3; // Sonic stands on it
    Obj_CPZBlock(obj);
    CHECK_EQ(obj->subtype, 6); // (kind 6: the fall)
    int16_t last = obj->y_pos.f.u;
    for (int i = 0; i < 30; i++)
        Obj_CPZBlock(obj);
    CHECK(obj->y_pos.f.u > last + 8);
    CHECK(obj->y_pos.f.u > y0);
}

// --- the alpha's Spiny (A5, A6) and Grabber (A7) ---

static void CPZObjects_TheSpinyWalksToAndFroAndThrowsAShotWhenSonicIsNear(void) {
    Reset();
    Sst *spiny = Spawn(0xA5, 0x32, 400, 300);
    player->x_pos.f.u = 0x1000; // (far)
    Obj_Spiny(spiny);
    CHECK_EQ(spiny->x_vel, -0x40);
    CHECK_EQ(spiny->collision_flags, 0x0B);
    for (int i = 0; i < 0x40; i++)
        Obj_Spiny(spiny);
    CHECK(spiny->x_pos.f.u < 400);               // (it walks to the left)
    CHECK_EQ(spiny->routine, 2);                   // (and does not look for Sonic for the first $80 frames)
    player->x_pos.f.u = (int16_t)(spiny->x_pos.f.u + 0x20);
    for (int i = 0; i < 0x50; i++)
        Obj_Spiny(spiny);
    CHECK_EQ(spiny->routine, 4);                   // (it has stopped to throw)
    CHECK_EQ(spiny->mapping_frame, 2);
    Sst *shot = NULL;
    for (int j = 0x21; j < 0x60; j++) {
        if (sst[j].code == sst_object_codes[0x98])
            shot = &sst[j];
    }
    for (int i = 0; i < 0x30 && shot == NULL; i++) {
        Obj_Spiny(spiny);
        for (int j = 0x21; j < 0x60; j++) {
            if (sst[j].code == sst_object_codes[0x98])
                shot = &sst[j];
        }
    }
    CHECK(shot != NULL);
    if (shot != NULL) {
        CHECK_EQ(shot->x_vel, 0x100);                // (towards Sonic, who is to its right)
        CHECK_EQ(shot->y_vel, -0x300);               // (and up)
        CHECK_EQ(shot->mapping_frame, 6);
    }
}

// The Grabber (A7, with the head A9, the claw A8 and the thread AA)
static void Frames(int n) {
    for (int i = 0; i < n; i++) {
        Sst_ClearQueues();
        Sst_Process(false);
    }
}

static Sst *Part(Sst *body, SstCode code) {
    for (int i = (int)(body - sst) + 1; i < SST_COUNT; i++)
        if (sst[i].code == code)
            return &sst[i];
    return NULL;
}

static Sst *GrabberSetUp(void) {
    Reset();
    player->code = NULL;
    Sst *grabber = Spawn(0xA7, 0x36, 500, 300);
    grabber->render_flags = 0; // (it goes to the left first)
    Frames(1);
    return grabber;
}

static void CPZObjects_TheGrabberGoesToAndFroAndMakesItsParts(void) {
    Sst *grabber = GrabberSetUp();
    CHECK_EQ(grabber->collision_flags, 0x0B);
    CHECK(Part(grabber, Obj_GrabberClaw) != NULL);
    CHECK(Part(grabber, Obj_GrabberHead) != NULL);
    CHECK(Part(grabber, Obj_GrabberStrand) != NULL);
    CHECK_EQ(grabber->x_vel, -0x40);
    Frames(0x100);
    CHECK(grabber->x_pos.f.u < 500 - 60);                // (a quarter of a pixel a frame, for 256 frames)
    CHECK_EQ(grabber->x_vel, 0x40);                      // (then back)
    CHECK(grabber->render_flags & SST_RENDER_X_FLIP);
    CHECK_EQ(Part(grabber, Obj_GrabberClaw)->x_pos.f.u, grabber->x_pos.f.u);
}

static void CPZObjects_TheGrabberGoesDownAndUpWhenACharacterIsUnderIt(void) {
    Sst *grabber = GrabberSetUp();
    player->x_pos.f.u = 500;
    player->y_pos.f.u = 340; // (below it, not more than $7F)
    Frames(1);
    CHECK_EQ(grabber->x_vel, 0); // (it waits...)
    Frames(0x11);
    CHECK_EQ(grabber->y_vel, 0x200); // (... $10 frames and goes)
    Frames(0x1F);
    CHECK_EQ(grabber->y_vel, 0x200);
    Frames(2);
    CHECK_EQ(grabber->y_vel, -0x200); // (half way, it turns)
    player->x_pos.f.u = 0x1000; // (he goes: it does not go for him again)
    Frames(0x20);
    CHECK_EQ(grabber->y_pos.f.u, 300);
    CHECK_EQ(grabber->x_vel, -0x40); // (and goes on)
    CHECK_EQ(Part(grabber, Obj_GrabberClaw)->y_pos.f.u, 300 + 0x10);
}

static void CPZObjects_TheGrabberDoesNotGoForWhoIsBesideItOrAboveIt(void) {
    Sst *grabber = GrabberSetUp();
    player->x_pos.f.u = 500 + 0x50;
    player->y_pos.f.u = 340;
    Frames(5);
    CHECK_EQ(grabber->x_vel, -0x40);
    player->x_pos.f.u = 500;
    player->y_pos.f.u = 250; // (above)
    Frames(5);
    CHECK_EQ(grabber->x_vel, -0x40);
    player->y_pos.f.u = 300 + 0x90; // (too far below)
    Frames(5);
    CHECK_EQ(grabber->x_vel, -0x40);
}

// Takes the character with the claw on the way down
static Sst *Seize(Sst *grabber) {
    player->x_pos.f.u = 500;
    player->y_pos.f.u = 340;
    Frames(0x18);
    Sst *claw = Part(grabber, Obj_GrabberClaw);
    claw->collision_property = 1; // (what a touch of Sonic's makes it)
    Frames(1);
    return claw;
}

static void CPZObjects_TheClawTakesWhoTouchesItAndTheGrabberCarriesHimUp(void) {
    Sst *grabber = GrabberSetUp();
    Sst *claw = Seize(grabber);
    CHECK_EQ(claw->collision_flags, 0);
    Frames(1);
    CHECK_EQ(OBJ_CONTROL(player), 0x81);
    CHECK_EQ(grabber->mapping_frame, 1); // (the claw closed)
    CHECK(grabber->y_vel < 0);
    Frames(1);
    CHECK_EQ(player->x_pos.f.u, claw->x_pos.f.u); // (he is where the claw is)
    CHECK_EQ(player->y_pos.f.u, claw->y_pos.f.u);
    Frames(0x40);
    CHECK_EQ(grabber->y_vel, 0);
    CHECK(grabber->y_pos.f.u <= 300);
}

static void CPZObjects_TheGrabberExplodesAndLetsGoAfterFlashing(void) {
    Sst *grabber = GrabberSetUp();
    Seize(grabber);
    Frames(120);
    CHECK(grabber->code == Obj_Grabber);
    CHECK_EQ(OBJ_CONTROL(player), 0x81);
    Frames(40);
    CHECK(grabber->code != Obj_Grabber); // (it is an explosion)
    CHECK_EQ(OBJ_CONTROL(player), 0);
    CHECK(player->status & SST_PSTATUS_IN_AIR);
    Frames(1);
    CHECK(Part(grabber, Obj_GrabberClaw) == NULL); // (the parts are gone with it)
}

static void CPZObjects_FourChangesOfSideInThirtyTwoFramesFreeWhoIsHeld(void) {
    Sst *grabber = GrabberSetUp();
    Seize(grabber);
    Frames(2);
    for (int i = 0; i < 40; i++) {
        jpad1_hold2 = (i & 1) ? JPAD_LEFT : JPAD_RIGHT;
        Frames(1);
        if (OBJ_CONTROL(player) == 0)
            break;
    }
    jpad1_hold2 = 0;
    CHECK_EQ(OBJ_CONTROL(player), 0);
    CHECK(player->status & SST_PSTATUS_IN_AIR);
    CHECK(grabber->code == Obj_Grabber); // (it goes on flashing, and explodes with nobody in it)
    Frames(200);
    CHECK(grabber->code != Obj_Grabber);
}

static void CPZObjects_HoldingOneSideDoesNotFreeWhoIsHeld(void) {
    Sst *grabber = GrabberSetUp();
    Seize(grabber);
    Frames(2);
    jpad1_hold2 = JPAD_LEFT;
    Frames(60);
    jpad1_hold2 = 0;
    CHECK_EQ(OBJ_CONTROL(player), 0x81);
    (void)grabber;
}


static int CountCode(SstCode code) {
    int n = 0;
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code == code)
            n++;
    return n;
}

static void CPZObjects_TheBossHasOneShipWhateverItDoes(void) {
    Reset();
    Sst *boss = Sst_Spawn(0x5D, 0x2B80, 0x4B0);
    CHECK(boss != NULL);
    Frames(1);
    const int parts = CountCode(boss->code);
    CHECK(parts >= 5 && parts <= 8);
    int ships = 0;
    for (int frame = 0; frame < 600; frame++) {
        Frames(1);
        ships = 0;
        for (int i = 0; i < SST_COUNT; i++)
            if (sst[i].code == boss->code && sst[i].routine == 2 && sst[i].collision_flags != 0xFF && sst[i].width_pixels == 0x20 && sst[i].mappings == boss->mappings)
                ships++;
        CHECK_EQ(ships, 1);
        if (ships != 1)
            break;
        player->x_pos.f.u = 0x2B00;
        player->y_pos.f.u = 0x520;
    }
}

// Sky Fortress's hang place ($D9) and conveyor wheels ($BA)
static void SFZObjects_AHangPlaceHoldsWhoIsInItAndAJumpLetsGoWithAThrow(void) {
    Reset();
    player->code = NULL;
    Sst *hang = Spawn(0xD9, 0, 500, 300);
    player->x_pos.f.u = 510;
    player->y_pos.f.u = 305;
    player->routine = 2;
    player->x_vel = 0x200;
    Frames(1);
    CHECK_EQ(OBJ_CONTROL(player), 1);
    CHECK_EQ(player->x_vel, 0);
    CHECK_EQ(player->y_pos.f.u, 300); // (at the bar's height)
    CHECK_EQ(player->anim, 0x14);
    Frames(30);
    CHECK_EQ(OBJ_CONTROL(player), 1); // (he stays)
    jpad1_press2 = JPAD_C;
    jpad1_hold2 = JPAD_RIGHT;
    Frames(1);
    jpad1_press2 = jpad1_hold2 = 0;
    CHECK_EQ(OBJ_CONTROL(player), 0);
    CHECK_EQ(player->y_vel, (int16_t)0xFD00);
    Frames(0x30); // (a direction held with the jump: $3C frames before he can take hold again)
    CHECK_EQ(OBJ_CONTROL(player), 0);
    Frames(0x20);
    CHECK_EQ(OBJ_CONTROL(player), 1);
    (void)hang;
}

static void SFZObjects_AHangPlaceLeavesWhoIsOutsideItAlone(void) {
    Reset();
    player->code = NULL;
    Spawn(0xD9, 0, 500, 300);
    player->routine = 2;
    player->x_pos.f.u = 500 + 0x40; // (beside it)
    player->y_pos.f.u = 305;
    Frames(3);
    CHECK_EQ(OBJ_CONTROL(player), 0);
    player->x_pos.f.u = 500;
    player->y_pos.f.u = 290; // (above it)
    Frames(3);
    CHECK_EQ(OBJ_CONTROL(player), 0);
    player->y_pos.f.u = 305;
    player->routine = 6; // (hurt)
    Frames(3);
    CHECK_EQ(OBJ_CONTROL(player), 0);
}

static void SFZObjects_TheConveyorWheelsStay(void) {
    Reset();
    Sst *wheels = Spawn(0xBA, 0, 500, 300);
    Frames(1);
    CHECK_EQ(wheels->width_pixels, 0x10);
    CHECK_EQ(wheels->collision_flags, 0);
    Frames(20);
    CHECK_EQ(wheels->x_pos.f.u, 500);
}

static void SFZObjects_ThePaletteSwitchSetsTheCycleByTheSideACharacterCrossesFrom(void) {
    Reset();
    player->code = NULL;
    Sst *sw = Spawn(0x8B, 1, 500, 300); // (it reaches $40 up and down)
    player->x_pos.f.u = 480;
    player->y_pos.f.u = 300;
    sfz_cycle_second = 0;
    Frames(2);
    CHECK_EQ(sfz_cycle_second, 0);
    player->x_pos.f.u = 510; // across, from the left: the second set
    Frames(1);
    CHECK_EQ(sfz_cycle_second, 1);
    player->x_pos.f.u = 490; // and back: the first
    Frames(1);
    CHECK_EQ(sfz_cycle_second, 0);
    player->y_pos.f.u = 300 + 0x50; // out of its reach: it does nothing
    player->x_pos.f.u = 510;
    Frames(1);
    CHECK_EQ(sfz_cycle_second, 0);
    sw->render_flags |= SST_RENDER_X_FLIP; // (flipped, it is the other way)
    player->y_pos.f.u = 300;
    player->x_pos.f.u = 490;
    Frames(1);
    player->x_pos.f.u = 510;
    Frames(1);
    CHECK_EQ(sfz_cycle_second, 0);
}

static void SFZObjects_TheCluckerPlatformIsSolidAndStays(void) {
    Reset();
    player->code = NULL;
    Sst *platform = Spawn(0xAD, 0, 500, 300);
    Frames(1);
    CHECK_EQ(platform->mapping_frame, 0x0C);
    player->routine = 2;
    player->x_pos.f.u = 500;
    player->y_radius = 0x13;
    player->x_radius = 9;
    player->y_pos.f.u = 300 - 8 - 0x13 + 2; // (landing on its top)
    player->y_vel = 0x100;
    player->status |= SST_PSTATUS_IN_AIR;
    Frames(4);
    CHECK(player->status & SST_PSTATUS_OBJECT_STAND);
}

static Sst *FindElevator(void) {
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code == Obj_SFZElevator && sst[i].routine == 4)
            return &sst[i];
    return NULL;
}

static void SFZObjects_AnElevatorMakerMakesAnElevatorThatOpensCarriesAndGoes(void) {
    Reset();
    player->code = NULL;
    Spawn(0xBD, 0x76, 500, 300);
    Frames(3);
    Sst *lift = FindElevator();
    CHECK(lift != NULL);
    if (lift == NULL)
        return;
    CHECK_EQ(lift->y_pos.f.u, 300); // (it opens first: it does not move)
    Frames(20);
    CHECK(lift->y_pos.f.u < 300); // (then it goes up, one a frame)
    const int16_t y = lift->y_pos.f.u;
    Frames(10);
    CHECK_EQ(lift->y_pos.f.u, y - 10);
    Frames(0xC8);
    CHECK(lift->code != Obj_SFZElevator || lift->routine_secondary >= 6); // (it closes and goes)
    Frames(20);
    int lifts = 0;
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code == Obj_SFZElevator && sst[i].routine == 4)
            lifts++;
    CHECK(lifts >= 1 && lifts <= 5); // (a new one every $40 frames, each gone after about $E0)
}

static void SFZObjects_AnElevatorOfSubtype78GoesDown(void) {
    Reset();
    player->code = NULL;
    Spawn(0xBD, 0x78, 500, 300);
    Frames(40);
    Sst *lift = FindElevator();
    CHECK(lift != NULL);
    if (lift != NULL)
        CHECK(lift->y_pos.f.u > 300);
}

static void SFZObjects_TheSpeedBoosterCarriesWhoStandsOnItAndThrowsHim(void) {
    Reset();
    player->code = NULL;
    Sst *booster = Spawn(0xC0, 4, 500, 300); // (it goes 64 pixels to the left)
    Frames(1);
    CHECK_EQ(booster->routine_secondary, 0);
    booster->status |= 0x08; // (he stands on it)
    player->routine = 2;
    player->x_pos.f.u = 500;
    player->y_pos.f.u = 300 - 0x11 - 0x13;
    Frames(2);
    CHECK_EQ(booster->routine_secondary, 2);
    CHECK_EQ(player->x_vel, 0);
    CHECK(player->status & SST_PSTATUS_X_FLIP); // (he faces the way it goes: left)
    int frames = 0;
    while (booster->routine_secondary == 2 && frames++ < 200) {
        booster->status |= 0x08;
        Frames(1);
    }
    CHECK_EQ(booster->routine_secondary, 4);
    CHECK_EQ(booster->x_pos.f.u, 500 - 64);
    CHECK_EQ(player->y_vel, (int16_t)0xFC00); // (thrown up, with its speed)
    CHECK(player->x_vel < 0);
}

static void SFZObjects_TheVerticalHelixHurtsAndTurns(void) {
    Reset();
    Sst *helix = Spawn(0xB4, 0, 500, 300);
    Frames(1);
    CHECK_EQ(helix->collision_flags, 0xA8);
    uint8_t seen = 0;
    for (int i = 0; i < 12; i++) {
        Frames(1);
        seen |= (uint8_t)(1 << helix->mapping_frame);
    }
    CHECK_EQ(seen, 7); // (its three frames)
}

static void SFZObjects_TheLaserWaitsToBeSeenThenCrossesLeftAndGoes(void) {
    Reset();
    scrpos_x.f.u = 400;
    Sst *laser = Spawn(0xB9, 0, 500, 300);
    Frames(1);
    laser->render_flags &= (uint8_t)~SST_RENDER_ON_SCREEN;
    Frames(3);
    CHECK_EQ(laser->x_pos.f.u, 500); // (nobody has seen it)
    laser->render_flags |= SST_RENDER_ON_SCREEN;
    Frames(1);
    Frames(2);
    CHECK(laser->x_pos.f.u < 500 - 16); // (16 a frame)
    for (int i = 0; i < 40 && laser->code != NULL; i++)
        Frames(1);
    CHECK(laser->code == NULL); // (gone past the edge of the picture)
}

static void SFZObjects_TheBossSwitchIsDestroyedByARollingLandingAndChangesTheLayout(void) {
    Reset();
    player->code = NULL;
    Sst *sw = Spawn(0xC2, 0, 500, 300);
    Frames(1);
    player->routine = 2;
    player->y_radius = 0x13;
    player->x_radius = 9;
    player->x_pos.f.u = 500;
    player->y_pos.f.u = 300 - 8 - 0x13 + 2;
    player->y_vel = 0x100;
    player->anim = 0; // (not rolling: it stays)
    player->status |= SST_PSTATUS_IN_AIR;
    Frames(3);
    CHECK(sw->code == Obj_SFZBossSwitch);
    player->anim = 2; // (rolling)
    player->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    player->status |= SST_PSTATUS_IN_AIR;
    player->y_pos.f.u = 300 - 8 - 0x13 + 2;
    player->y_vel = 0x100;
    Frames(4);
    CHECK(sw->code != Obj_SFZBossSwitch);
}

static void SFZObjects_TheHorizontalHelixCatchesWhoIsOverItWhileItSpinsAtFullSpeed(void) {
    Reset();
    player->code = NULL;
    Sst *helix = Spawn(0xB5, 0x5E, 500, 300);
    Frames(1);
    CHECK_EQ(helix->routine, 2);
    CHECK_EQ(helix->anim, 4); // (it starts at its full speed)
    player->routine = 2;
    player->x_pos.f.u = 500;
    player->y_pos.f.u = 300 - 0x30;
    Frames(1);
    CHECK(player->status & SST_PSTATUS_IN_AIR);
    CHECK_EQ(player->anim, 0x0F);
    helix->anim = 5; // (slowing: it does not)
    Scratch_Sonic *sc = (Scratch_Sonic *)player->own.u8;
    sc->flip_angle = 0;
    player->anim = 0;
    Frames(1);
    CHECK_EQ(player->anim, 0);
}

static void SFZObjects_APlatformHelixIsAPlatformThatFoldsAwayAndComesBack(void) {
    Reset();
    player->code = NULL;
    Sst *helix = Spawn(0xB6, 0x62, 500, 300);
    Frames(2);
    CHECK_EQ(helix->routine, 2);
    CHECK_EQ(helix->routine_secondary, 2); // (a platform for a while: under $80 frames)
    Frames(0x90);
    CHECK(helix->routine_secondary == 4 || helix->routine_secondary == 6 || helix->routine_secondary == 2); // (it folds: and back to a platform after $C0)
    bool folded = false;
    for (int i = 0; i < 0x200 && !folded; i++) {
        Frames(1);
        folded = helix->routine_secondary == 4;
    }
    CHECK(folded);
}

static void SFZObjects_TheLargeLaserFlickersForThirtyTwoFramesAndGoes(void) {
    Reset();
    Sst *laser = Spawn(0xB7, 0x6A, 500, 300);
    Frames(1);
    CHECK_EQ(laser->collision_flags, 0xA9);
    Frames(40);
    CHECK(laser->code == NULL);
}

static Sst *FindShot(void) {
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code == Obj_EnemyWeapon)
            return &sst[i];
    return NULL;
}

static void SFZObjects_TheCannonTurnsToWhoIsBelowItAndFiresEverySixtyFourFrames(void) {
    Reset();
    player->code = NULL;
    Sst *cannon = Spawn(0xB8, 0x6C, 500, 300);
    player->x_pos.f.u = 500 + 0x50; // (to its right, below it)
    player->y_pos.f.u = 400;
    Frames(1);
    cannon->render_flags |= SST_RENDER_ON_SCREEN;
    Frames(1);
    CHECK_EQ(cannon->routine, 4);
    Frames(2);
    CHECK_EQ(cannon->mapping_frame, 2); // (it turns to him)
    Sst *shot = FindShot();
    CHECK(shot != NULL);
    if (shot != NULL) {
        CHECK_EQ(shot->x_vel, 0x100);
        CHECK_EQ(shot->y_vel, 0x100);
    }
    CHECK_EQ(cannon->collision_flags, 0x86);
}

static void SFZObjects_TheCannonDoesNotFireAtWhoIsAboveItOrFarOff(void) {
    Reset();
    player->code = NULL;
    Sst *cannon = Spawn(0xB8, 0x6C, 500, 300);
    player->x_pos.f.u = 500;
    player->y_pos.f.u = 200; // (above)
    Frames(1);
    cannon->render_flags |= SST_RENDER_ON_SCREEN;
    Frames(3);
    CHECK_EQ(cannon->routine, 2);
    player->y_pos.f.u = 400;
    player->x_pos.f.u = 500 + 0x70; // (too far to the side)
    cannon->render_flags |= SST_RENDER_ON_SCREEN;
    Frames(3);
    CHECK_EQ(cannon->routine, 2);
}

static void SFZObjects_TheBreakableWallHoldsWhoTouchesItAndBreaksIntoFourPieces(void) {
    Reset();
    player->code = NULL;
    Sst *wall = Spawn(0xC1, 2, 500, 300); // (two seconds)
    Frames(1);
    CHECK_EQ(wall->collision_flags, 0xE1);
    player->routine = 2;
    player->x_pos.f.u = 500 - 0x10; // (right of the grab place, which is $14 before the wall)
    player->y_pos.f.u = 300;
    wall->collision_property = 1; // (what a touch of Sonic's makes it)
    Frames(1);
    CHECK_EQ(OBJ_CONTROL(player), 1);
    CHECK_EQ(player->x_pos.f.u, 500 - 0x14);
    CHECK_EQ(player->anim, 0x11);
    jpad1_hold2 = JPAD_UP;
    Frames(5);
    CHECK_EQ(player->y_pos.f.u, 295);
    jpad1_hold2 = 0;
    jpad1_press2 = JPAD_A; // (a jump lets go, and the wall breaks)
    Frames(1);
    jpad1_press2 = 0;
    CHECK_EQ(OBJ_CONTROL(player), 0);
    int pieces = 0;
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code == Obj_SFZBreakableWall && sst[i].routine == 4)
            pieces++;
    CHECK_EQ(pieces, 4);
}

static void SFZObjects_TheBreakableWallLetsGoWhenItsTimeIsOut(void) {
    Reset();
    player->code = NULL;
    Sst *wall = Spawn(0xC1, 1, 500, 300); // (one second: $3C frames)
    Frames(1);
    player->routine = 2;
    player->x_pos.f.u = 500 - 0x10;
    player->y_pos.f.u = 300;
    wall->collision_property = 1;
    Frames(1);
    CHECK_EQ(OBJ_CONTROL(player), 1);
    Frames(0x3C);
    CHECK_EQ(OBJ_CONTROL(player), 0);
}

static void SFZObjects_TheCloudsDriftLeftAtTheirOwnSpeedsAndTheRestStay(void) {
    Reset();
    scrpos_x.f.u = 400;
    Sst *near_cloud = Spawn(0xB3, 0x56, 500, 300);
    Sst *far_cloud = Spawn(0xB3, 0x5A, 500, 320);
    Frames(1);
    CHECK_EQ(near_cloud->mapping_frame, 0);
    CHECK_EQ(far_cloud->mapping_frame, 2);
    CHECK_EQ(near_cloud->x_vel, (int16_t)0xFF80);
    CHECK_EQ(far_cloud->x_vel, (int16_t)0xFFE0);
    Frames(256);
    CHECK_EQ(near_cloud->x_pos.f.u, 500 - 128);
    CHECK_EQ(far_cloud->x_pos.f.u, 500 - 32);
    Sst *hook = Spawn(0xBB, 0, 500, 300);
    Frames(3);
    CHECK_EQ(hook->collision_flags, 0x09);
    CHECK_EQ(hook->x_pos.f.u, 500);
}

static void SFZObjects_TheRocketBoostShowsEveryOtherFrameAndTheEnginePartTurns(void) {
    Reset();
    Sst *boost = Spawn(0xBC, 0, 500, 300);
    Sst *part = Spawn(0xBF, 0, 520, 300);
    Frames(1);
    uint8_t seen = 0;
    for (int i = 0; i < 12; i++) {
        Frames(1);
        seen |= (uint8_t)(1 << part->mapping_frame);
    }
    CHECK_EQ(seen, 7);
    CHECK_EQ(boost->width_pixels, 0x10);
}

static void SFZObjects_AHatchwayOpensToBeStoodOnAndShutsAgain(void) {
    Reset();
    player->code = NULL;
    Sst *hatch = Spawn(0xBE, 0, 500, 300);
    Frames(1);
    bool open = false, shut_after = false;
    for (int i = 0; i < 0x200; i++) {
        Frames(1);
        if (hatch->mapping_frame == 3 || hatch->mapping_frame == 4)
            open = true;
        else if (open && hatch->mapping_frame == 0)
            shut_after = true;
    }
    CHECK(open);
    CHECK(shut_after);
}

static void SkyBadniks_TheBalkiryGoesLeftWithItsBoostAndTheBoostGoesWhenItIsGone(void) {
    Reset();
    scrpos_x.f.u = 400;
    Sst *balkiry = Spawn(0xAC, 0, 500, 300);
    Frames(2);
    CHECK_EQ(balkiry->collision_flags, 0x08);
    CHECK_EQ(balkiry->x_vel, (int16_t)0xFD00);
    Sst *boost = NULL;
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code == Obj_EnemyBoost)
            boost = &sst[i];
    CHECK(boost != NULL);
    Frames(20);
    CHECK(balkiry->x_pos.f.u < 500 - 50);
    if (boost != NULL) {
        CHECK_EQ(boost->x_pos.f.u, balkiry->x_pos.f.u); // (the boost is where it is)
        balkiry->code = Obj_Explosion;
        Frames(2);
        CHECK(boost->code == NULL);
    }
}

static void SkyBadniks_TheWeaponObjectHasTheTurtloidsBulletAndTheNebulasBomb(void) {
    Reset();
    scrpos_x.f.u = 400;
    Sst *bomb = Spawn(0x98, 0, 500, 300);
    ((Own_Weapon *)bomb->own.u8)->weapon = Weapon_NebulaBomb;
    Frames(1);
    CHECK_EQ(bomb->collision_flags, 0x8B);
    bomb->render_flags |= SST_RENDER_ON_SCREEN;
    const uint16_t art = bomb->art_tile;
    Frames(1);
    CHECK(bomb->art_tile != art); // (its palette line flickers)
    CHECK(bomb->y_vel > 0); // (and it falls)
    Sst *bullet = Spawn(0x98, 0, 520, 300);
    ((Own_Weapon *)bullet->own.u8)->weapon = Weapon_TurtloidShot;
    Frames(1);
    CHECK_EQ(bullet->collision_flags, 0x98);
}

void RegisterCPZObjectTests(void) {
    RUN_TEST(CPZObjects_BarrierRisesWhileSonicIsNearAndSinksAfter);
    RUN_TEST(CPZObjects_BarrierDoesNotOpenForTheFarSide);
    RUN_TEST(CPZObjects_ABoosterGivesTheSpeedItsSubtypeSays);
    RUN_TEST(CPZObjects_ABoosterLeavesWhoIsInTheAirAlone);
    RUN_TEST(CPZObjects_ABlockFallsOnceItIsStoodOn);
    RUN_TEST(CPZObjects_TheSpinyWalksToAndFroAndThrowsAShotWhenSonicIsNear);
    RUN_TEST(CPZObjects_TheGrabberGoesToAndFroAndMakesItsParts);
    RUN_TEST(CPZObjects_TheGrabberGoesDownAndUpWhenACharacterIsUnderIt);
    RUN_TEST(CPZObjects_TheGrabberDoesNotGoForWhoIsBesideItOrAboveIt);
    RUN_TEST(CPZObjects_TheClawTakesWhoTouchesItAndTheGrabberCarriesHimUp);
    RUN_TEST(CPZObjects_TheGrabberExplodesAndLetsGoAfterFlashing);
    RUN_TEST(CPZObjects_FourChangesOfSideInThirtyTwoFramesFreeWhoIsHeld);
    RUN_TEST(CPZObjects_HoldingOneSideDoesNotFreeWhoIsHeld);
    RUN_TEST(CPZObjects_TheBossHasOneShipWhateverItDoes);
    RUN_TEST(SFZObjects_AHangPlaceHoldsWhoIsInItAndAJumpLetsGoWithAThrow);
    RUN_TEST(SFZObjects_AHangPlaceLeavesWhoIsOutsideItAlone);
    RUN_TEST(SFZObjects_TheConveyorWheelsStay);
    RUN_TEST(SFZObjects_ThePaletteSwitchSetsTheCycleByTheSideACharacterCrossesFrom);
    RUN_TEST(SFZObjects_TheCluckerPlatformIsSolidAndStays);
    RUN_TEST(SFZObjects_AnElevatorMakerMakesAnElevatorThatOpensCarriesAndGoes);
    RUN_TEST(SFZObjects_AnElevatorOfSubtype78GoesDown);
    RUN_TEST(SFZObjects_TheSpeedBoosterCarriesWhoStandsOnItAndThrowsHim);
    RUN_TEST(SFZObjects_TheVerticalHelixHurtsAndTurns);
    RUN_TEST(SFZObjects_TheHorizontalHelixCatchesWhoIsOverItWhileItSpinsAtFullSpeed);
    RUN_TEST(SFZObjects_APlatformHelixIsAPlatformThatFoldsAwayAndComesBack);
    RUN_TEST(SFZObjects_TheLargeLaserFlickersForThirtyTwoFramesAndGoes);
    RUN_TEST(SFZObjects_TheCannonTurnsToWhoIsBelowItAndFiresEverySixtyFourFrames);
    RUN_TEST(SFZObjects_TheCannonDoesNotFireAtWhoIsAboveItOrFarOff);
    RUN_TEST(SFZObjects_TheBreakableWallHoldsWhoTouchesItAndBreaksIntoFourPieces);
    RUN_TEST(SFZObjects_TheBreakableWallLetsGoWhenItsTimeIsOut);
    RUN_TEST(SFZObjects_TheCloudsDriftLeftAtTheirOwnSpeedsAndTheRestStay);
    RUN_TEST(SFZObjects_TheRocketBoostShowsEveryOtherFrameAndTheEnginePartTurns);
    RUN_TEST(SFZObjects_AHatchwayOpensToBeStoodOnAndShutsAgain);
    RUN_TEST(SkyBadniks_TheBalkiryGoesLeftWithItsBoostAndTheBoostGoesWhenItIsGone);
    RUN_TEST(SkyBadniks_TheWeaponObjectHasTheTurtloidsBulletAndTheNebulasBomb);
    RUN_TEST(SFZObjects_TheLaserWaitsToBeSeenThenCrossesLeftAndGoes);
    RUN_TEST(SFZObjects_TheBossSwitchIsDestroyedByARollingLandingAndChangesTheLayout);
}
