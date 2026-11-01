#include "test.h"

#include <string.h>

#include "Level.h"
#include "Object.h"
#include "SstLoad.h"
#include "Object/CPZBadniks.h"
#include "Object/CPZObjects.h"
#include "Object/Coconuts.h"
#include "Object/Sonic.h"

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

static void CPZObjects_TheGrabberOnlyStays(void) {
    Reset();
    Sst *grabber = Spawn(0xA7, 0x36, 300, 300);
    Obj_Grabber(grabber);
    CHECK_EQ(grabber->collision_flags, 0x0B);
    for (int i = 0; i < 20; i++)
        Obj_Grabber(grabber);
    CHECK_EQ(grabber->x_pos.f.u, 300);
    CHECK_EQ(grabber->x_vel, 0);
}

void RegisterCPZObjectTests(void) {
    RUN_TEST(CPZObjects_BarrierRisesWhileSonicIsNearAndSinksAfter);
    RUN_TEST(CPZObjects_BarrierDoesNotOpenForTheFarSide);
    RUN_TEST(CPZObjects_ABoosterGivesTheSpeedItsSubtypeSays);
    RUN_TEST(CPZObjects_ABoosterLeavesWhoIsInTheAirAlone);
    RUN_TEST(CPZObjects_ABlockFallsOnceItIsStoodOn);
    RUN_TEST(CPZObjects_TheSpinyWalksToAndFroAndThrowsAShotWhenSonicIsNear);
    RUN_TEST(CPZObjects_TheGrabberOnlyStays);
}
