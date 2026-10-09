#include "test.h"

#include <string.h>

#include "Level.h"
#include "Object.h"
#include "Object/HTZObjects.h"
#include "Object/Sonic.h"

// Hill Top's breakable floor (Obj_0x2F): it breaks for a roller standing on it who walks the secondary collision path (the solid bits $E/$F that the path swappers give each character), or with subtype
// bit 7 for any roller; whoever stands on it and does not break it is put back on the first path ($C/$D).

static Object *Floor(uint8_t subtype) {
    memset(objects, 0, sizeof(Object) * 0x40);
    scrpos_x.v = scrpos_y.v = 0; // (the camera at the level's start: the floor is near it, so it is not forgotten for being out of range)
    Object *obj = &objects[0x20];
    obj->type = 0x2F;
    obj->scratch.u8[0] = subtype;
    obj->pos.l.x.f.u = 100;
    obj->pos.l.y.f.u = 200;
    obj->render.f.on_screen = true;
    obj->status.b = 1 << 3; // Sonic is standing on it
    Obj_HTZBreakFloor(obj); // (its first call sets it up)
    obj->status.b = 1 << 3;
    obj->render.f.on_screen = true;
    return obj;
}

static void Roller(uint8_t top_solid_bit) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)&player->scratch;
    player->pos.l.x.f.u = 100;
    player->pos.l.y.f.u = 200 - 0x25 - 19;
    player->y_rad = 14;
    player->routine = 2;
    player->anim = SonAnimId_Roll;
    player->status.b = 0;
    player->status.p.f.object_stand = true;
    player->status.p.f.in_ball = true;
    scratch->top_solid_bit = top_solid_bit;
    scratch->lrb_solid_bit = (uint8_t)(top_solid_bit + 1);
}

static void HTZFloor_ABallOnTheSecondPathBreaksIt(void) {
    Object *obj = Floor(0);
    Roller(0xE);
    Obj_HTZBreakFloor(obj);
    CHECK_EQ(obj->routine, 4); // it has become its first piece
    CHECK(player->status.p.f.in_air);
    CHECK(!player->status.p.f.object_stand);
}

static void HTZFloor_ABallOnTheFirstPathDoesNotBreakIt(void) {
    Object *obj = Floor(0);
    Roller(0xC);
    Obj_HTZBreakFloor(obj);
    CHECK_EQ(obj->routine, 2);
    CHECK(!player->status.p.f.in_air);
    CHECK_EQ(((Scratch_Sonic *)&player->scratch)->top_solid_bit, 0xC);
}

static void HTZFloor_SubtypeBitSevenBreaksItForAnyRoller(void) {
    Object *obj = Floor(0x80);
    Roller(0xC);
    Obj_HTZBreakFloor(obj);
    CHECK_EQ(obj->routine, 4);
}

static void HTZFloor_AWalkerOnTheSecondPathIsPutBackOnTheFirst(void) {
    Object *obj = Floor(0);
    Roller(0xE);
    player->anim = SonAnimId_Walk;
    player->status.p.f.in_ball = false;
    Obj_HTZBreakFloor(obj);
    CHECK_EQ(obj->routine, 2);
    CHECK_EQ(((Scratch_Sonic *)&player->scratch)->top_solid_bit, 0xC);
    CHECK_EQ(((Scratch_Sonic *)&player->scratch)->lrb_solid_bit, 0xD);
}

void RegisterHTZFloorTests(void) {
    RUN_TEST(HTZFloor_ABallOnTheSecondPathBreaksIt);
    RUN_TEST(HTZFloor_ABallOnTheFirstPathDoesNotBreakIt);
    RUN_TEST(HTZFloor_SubtypeBitSevenBreaksItForAnyRoller);
    RUN_TEST(HTZFloor_AWalkerOnTheSecondPathIsPutBackOnTheFirst);
}
