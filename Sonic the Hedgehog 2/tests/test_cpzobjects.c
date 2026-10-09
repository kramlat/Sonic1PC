#include "test.h"

#include <string.h>

#include "Level.h"
#include "Object.h"
#include "Object/CPZObjects.h"
#include "Object/Sonic.h"

// Chemical Plant's objects: the one way barrier (2D), the speed booster (1B) and the staircase blocks (6B), as the prototype's routines have them.

static void Reset(void) {
    memset(objects, 0, sizeof(Object) * 0x60);
    level_id = LEVEL_ID(ZoneId_CPZ, 0);
    scrpos_x.v = scrpos_y.v = 0x100 << 16; // (near the objects below: they are not forgotten for being far away)
    limit_btm1 = 0x720;
    player->routine = 2;
    player->pos.l.x.f.u = 0x1000; // (far from everything, until a test puts him somewhere)
    player->pos.l.y.f.u = 0x1000;
}

static Object *Spawn(uint8_t type, uint8_t subtype, int16_t x, int16_t y) {
    Object *obj = &objects[0x20];
    memset(obj, 0, sizeof(Object));
    obj->type = type;
    obj->scratch.u8[0] = subtype;
    obj->pos.l.x.f.u = x;
    obj->pos.l.y.f.u = y;
    return obj;
}

// The barrier: it rises 8 a frame (to $40) while a character is in the strip $200 wide on its open side, and sinks again when nobody is
// (a barrier is at least $200 from the left of the level, as the strip's left edge is an unsigned x)
static void CPZObjects_BarrierRisesWhileSonicIsNearAndSinksAfter(void) {
    Reset();
    Object *obj = Spawn(0x2D, 0, 800, 300);
    Obj_CPZBarrier(obj);
    CHECK_EQ(obj->pos.l.y.f.u, 300);
    player->pos.l.x.f.u = 700; // inside [x - $200, x)
    for (int i = 0; i < 4; i++)
        Obj_CPZBarrier(obj);
    CHECK_EQ(obj->pos.l.y.f.u, 300 - 4 * 8);
    for (int i = 0; i < 20; i++)
        Obj_CPZBarrier(obj);
    CHECK_EQ(obj->pos.l.y.f.u, 300 - 0x40); // (and no higher)
    player->pos.l.x.f.u = 0x1000; // gone
    for (int i = 0; i < 4; i++)
        Obj_CPZBarrier(obj);
    CHECK_EQ(obj->pos.l.y.f.u, 300 - 0x40 + 4 * 8);
    for (int i = 0; i < 20; i++)
        Obj_CPZBarrier(obj);
    CHECK_EQ(obj->pos.l.y.f.u, 300);
}

static void CPZObjects_BarrierDoesNotOpenForTheFarSide(void) {
    Reset();
    Object *obj = Spawn(0x2D, 0, 800, 300);
    Obj_CPZBarrier(obj);
    player->pos.l.x.f.u = 900; // beyond it, the side it shuts
    for (int i = 0; i < 10; i++)
        Obj_CPZBarrier(obj);
    CHECK_EQ(obj->pos.l.y.f.u, 300);
}

static void CPZObjects_ABoosterGivesTheSpeedItsSubtypeSays(void) {
    Reset();
    Object *obj = Spawn(0x1B, 0, 200, 300);
    player->pos.l.x.f.u = 200;
    player->pos.l.y.f.u = 300;
    player->status.p.f.in_air = false;
    Obj_CPZBooster(obj);
    CHECK_EQ(player->xsp, 0x1000);
    CHECK_EQ(player->inertia, 0x1000);
    CHECK_EQ(((Scratch_Sonic *)&player->scratch)->control_lock, 0xF);

    Object *slow = Spawn(0x1B, 2, 200, 300); // bit 1: the slower one
    player->xsp = 0;
    Obj_CPZBooster(slow);
    CHECK_EQ(player->xsp, 0xA00);

    Object *back = Spawn(0x1B, 0, 200, 300);
    Obj_CPZBooster(back); // (its first call sets it up)
    back->status.o.f.x_flip = true;
    Obj_CPZBooster(back);
    CHECK_EQ(player->xsp, -0x1000);
    CHECK(player->status.p.f.x_flip);
}

static void CPZObjects_ABoosterLeavesWhoIsInTheAirAlone(void) {
    Reset();
    Object *obj = Spawn(0x1B, 0, 200, 300);
    player->pos.l.x.f.u = 200;
    player->pos.l.y.f.u = 300;
    player->status.p.f.in_air = true;
    player->xsp = 0x123;
    Obj_CPZBooster(obj);
    CHECK_EQ(player->xsp, 0x123);
}

// The staircase block of kind 5 waits to be stood on, and then (kind 6) falls
static void CPZObjects_ABlockFallsOnceItIsStoodOn(void) {
    Reset();
    Object *obj = Spawn(0x6B, 5, 300, 300);
    Obj_CPZBlock(obj);
    const int16_t y0 = obj->pos.l.y.f.u;
    obj->status.b |= 1 << 3; // Sonic stands on it
    Obj_CPZBlock(obj);
    CHECK_EQ(obj->scratch.u8[0], 6); // (kind 6: the fall)
    int16_t last = obj->pos.l.y.f.u;
    for (int i = 0; i < 30; i++)
        Obj_CPZBlock(obj);
    CHECK(obj->pos.l.y.f.u > last + 8);
    CHECK(obj->pos.l.y.f.u > y0);
}

void RegisterCPZObjectTests(void) {
    RUN_TEST(CPZObjects_BarrierRisesWhileSonicIsNearAndSinksAfter);
    RUN_TEST(CPZObjects_BarrierDoesNotOpenForTheFarSide);
    RUN_TEST(CPZObjects_ABoosterGivesTheSpeedItsSubtypeSays);
    RUN_TEST(CPZObjects_ABoosterLeavesWhoIsInTheAirAlone);
    RUN_TEST(CPZObjects_ABlockFallsOnceItIsStoodOn);
}
