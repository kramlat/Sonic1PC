#include "test.h"

#include <string.h>

#include "Level.h"
#include "Object.h"
#include "SstLoad.h"
#include "Object/CPZBadniks.h"
#include "Object/CPZObjects.h"
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
}
