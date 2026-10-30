#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/Sonic.h"
#include "Object/Spring.h"
#include "ObjectSst.h"
#include "SstLoad.h"

// Object 41, the spring: red ($F00 power $1000, yellow $A00), up, sideways and down by the subtype, and the launch when Sonic has landed on it (SolidObject sets routine_secondary) or pushed it.

static Sst *Spawn(uint8_t subtype) {
    memset(player, 0, sizeof(Object));
    player->routine = 2;
    player->y_rad = 19;
    player->pos.l.x.f.u = 0x1000; // far, until a test puts him on it
    player->pos.l.y.f.u = 0x1000;
    scrpos_x.f.u = 0xF00;
    scrpos_y.f.u = 0xF00;
    Sst_Reset();
    static const SstObjectIndex index_ = {sst_object_codes, 0x100};
    SstLoadCamera camera = {0xF00, 0xF00, false, 0xFFF};
    SstLoad_Init(&index_, NULL, false, &camera);
    Sst *obj = Sst_Spawn(ObjId_Spring, 0x1000, 0x1040);
    obj->subtype = subtype;
    Obj_Spring(obj);
    return obj;
}

static void Spring_PowerComesFromTheColour(void) {
    Sst *red = Spawn(0);
    CHECK_EQ(((Own_Spring *)red->own.u8)->power, -0x1000);
    Sst *yellow = Spawn(2);
    CHECK_EQ(((Own_Spring *)yellow->own.u8)->power, -0xA00);
}

static void Spring_SubtypeChoosesTheDirection(void) {
    Sst *up = Spawn(0);
    CHECK_EQ(up->routine, 2);
    CHECK_EQ(up->width_pixels, 16);

    Sst *side = Spawn(0x10);
    CHECK_EQ(side->routine, 8);
    CHECK_EQ(side->mapping_frame, 3);
    CHECK_EQ(side->width_pixels, 8);

    Sst *down = Spawn(0x20);
    CHECK_EQ(down->routine, 14);
    CHECK(down->status & SST_STATUS_Y_FLIP);
}

static void Spring_LaunchesSonicUpWhenHeLandedOnIt(void) {
    Sst *obj = Spawn(0);
    player->pos.l.x.f.u = 0x1000;
    player->pos.l.y.f.u = 0x1040 - 16 - 19;
    player->status.p.f.in_air = false;
    player->status.p.f.object_stand = true;
    obj->status |= SST_STATUS_PLAYER_STAND;
    obj->routine_secondary = 1;
    Obj_Spring(obj);
    CHECK_EQ(player->ysp, -0x1000);
    CHECK(player->status.p.f.in_air);
    CHECK(!player->status.p.f.object_stand);
    CHECK_EQ(player->anim, SonAnimId_Spring);
    CHECK_EQ(obj->routine, 4); // (the spring bounces)
}

static void Spring_ThrowsSonicBackFromASideSpringHeRunsInto(void) {
    Sst *obj = Spawn(0x10);
    player->pos.l.x.f.u = 0x1000 - 30; // to its left, running right into it
    player->pos.l.y.f.u = 0x1040;
    player->xsp = player->inertia = 0x600;
    player->status.p.f.in_air = false;
    for (int i = 0; i < 12 && player->xsp > 0; i++) {
        Obj_Spring(obj);
        if (player->xsp > 0)
            player->pos.l.x.f.u += 2;
    }
    CHECK_EQ(player->xsp, 0x1000); // (at the spring's power, the way the object's flip says: an unflipped one throws to the right, a flipped one the other way)
    CHECK_EQ(((Scratch_Sonic *)&player->scratch)->control_lock, 15);
    CHECK_EQ(player->inertia, player->xsp);
}

static void Spring_AFlippedSideSpringThrowsTheOtherWay(void) {
    Sst *obj = Spawn(0x10);
    obj->status |= SST_STATUS_X_FLIP;
    player->pos.l.x.f.u = 0x1000 + 30;
    player->pos.l.y.f.u = 0x1040;
    player->xsp = player->inertia = -0x600;
    player->status.p.f.in_air = false;
    for (int i = 0; i < 12 && player->xsp < 0; i++) {
        Obj_Spring(obj);
        if (player->xsp < 0)
            player->pos.l.x.f.u -= 2;
    }
    CHECK_EQ(player->xsp, -0x1000);
}

void RegisterSpringTests(void) {
    RUN_TEST(Spring_PowerComesFromTheColour);
    RUN_TEST(Spring_SubtypeChoosesTheDirection);
    RUN_TEST(Spring_LaunchesSonicUpWhenHeLandedOnIt);
    RUN_TEST(Spring_ThrowsSonicBackFromASideSpringHeRunsInto);
    RUN_TEST(Spring_AFlippedSideSpringThrowsTheOtherWay);
}
