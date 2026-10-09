#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/Sonic.h"
// (Spring.h would define the object's art a second time: its scratch layout is repeated here)
typedef struct {
    uint8_t subtype; // 0x28
    uint8_t pad[7];  // 0x29-0x2F
    int16_t power;   // 0x30
} Scratch_Spring;

void Obj_Spring(Object *obj);

// Object 41, the spring: red ($F00 power $1000, yellow $A00), up, sideways and down by the subtype, and the launch when Sonic has landed on it (SolidObject sets routine_sec) or pushed it.

static Object *Spawn(uint8_t subtype) {
    memset(player, 0, sizeof(Object));
    player->routine = 2;
    player->y_rad = 19;
    player->pos.l.x.f.u = 0x1000; // far, until a test puts him on it
    player->pos.l.y.f.u = 0x1000;
    scrpos_x.f.u = 0xF00;
    scrpos_y.f.u = 0xF00;
    Object *obj = &level_objects[4];
    memset(obj, 0, sizeof(Object));
    obj->type = ObjId_Spring;
    obj->scratch.u8[0] = subtype;
    obj->pos.l.x.f.u = 0x1000;
    obj->pos.l.y.f.u = 0x1040;
    Obj_Spring(obj);
    return obj;
}

static void Spring_PowerComesFromTheColour(void) {
    Object *red = Spawn(0);
    CHECK_EQ(((Scratch_Spring *)&red->scratch)->power, -0x1000);
    Object *yellow = Spawn(2);
    CHECK_EQ(((Scratch_Spring *)&yellow->scratch)->power, -0xA00);
}

static void Spring_SubtypeChoosesTheDirection(void) {
    Object *up = Spawn(0);
    CHECK_EQ(up->routine, 2);
    CHECK_EQ(up->width_pixels, 16);

    Object *side = Spawn(0x10);
    CHECK_EQ(side->routine, 8);
    CHECK_EQ(side->frame, 3);
    CHECK_EQ(side->width_pixels, 8);

    Object *down = Spawn(0x20);
    CHECK_EQ(down->routine, 14);
    CHECK(down->status.o.f.y_flip);
}

static void Spring_LaunchesSonicUpWhenHeLandedOnIt(void) {
    Object *obj = Spawn(0);
    player->pos.l.x.f.u = 0x1000;
    player->pos.l.y.f.u = 0x1040 - 16 - 19;
    player->status.p.f.in_air = false;
    player->status.p.f.object_stand = true;
    obj->status.o.f.player_stand = true;
    obj->routine_sec = 1;
    Obj_Spring(obj);
    CHECK_EQ(player->ysp, -0x1000);
    CHECK(player->status.p.f.in_air);
    CHECK(!player->status.p.f.object_stand);
    CHECK_EQ(player->anim, SonAnimId_Spring);
    CHECK_EQ(obj->routine, 4); // (the spring bounces)
}

static void Spring_ThrowsSonicBackFromASideSpringHeRunsInto(void) {
    Object *obj = Spawn(0x10);
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
    Object *obj = Spawn(0x10);
    obj->status.o.f.x_flip = true;
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
