#include "test.h"

#include <string.h>

#include "Level.h"
#include "Object.h"
#include "Object/Sonic.h"
#include "Solid.h"

// Solid_Character, the prototype's SolidObject family. A character standing on a sloped solid (a lever spring, a diagonal spring) is put where the slope table says its top is: at the object's y minus the
// table's height at his column. The first version of this added the table's first entry instead of taking it away and sank him by twice that entry (16 pixels into a lever spring).

static Object *Reset(void) {
    memset(objects, 0, sizeof(Object) * 4);
    return &objects[0x20]; // a level object slot
}

static void Solid_StandingOnASlopeIsAtTheTablesHeight(void) {
    static const int8_t slope[0x20] = { 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8 };
    Object *obj = Reset();
    obj->pos.l.x.f.u = 100;
    obj->pos.l.y.f.u = 200;
    obj->status.b = 1 << 3; // Sonic stands on it
    player->pos.l.x.f.u = 100;
    player->pos.l.y.f.u = 0;
    player->y_rad = 19;
    player->status.p.f.object_stand = true;
    player->status.p.f.in_air = false;
    player->routine = 2;

    Solid_Character(obj, player, SolidChar_Sonic, 0x10, 0x10, 0x11, 100, slope);
    CHECK_EQ(player->pos.l.y.f.u, 200 - 8 - 19); // (the table's value 8 above the object's y, then his own radius: he stands on it, not 16 pixels in it)
}

static void Solid_StandingOnAFlatTopIsAtItsWalkingHeight(void) {
    Object *obj = Reset();
    obj->pos.l.x.f.u = 100;
    obj->pos.l.y.f.u = 200;
    obj->status.b = 1 << 3;
    player->pos.l.x.f.u = 100;
    player->y_rad = 19;
    player->status.p.f.object_stand = true;
    player->routine = 2;

    Solid_Character(obj, player, SolidChar_Sonic, 0x10, 0x10, 0x11, 100, NULL);
    CHECK_EQ(player->pos.l.y.f.u, 200 - 0x11 - 19);
}

static void Solid_WalkingOffTheEdgeLetsGo(void) {
    Object *obj = Reset();
    obj->pos.l.x.f.u = 100;
    obj->pos.l.y.f.u = 200;
    obj->status.b = 1 << 3;
    player->pos.l.x.f.u = 100 + 0x40; // far past the edge
    player->y_rad = 19;
    player->status.p.f.object_stand = true;
    player->routine = 2;

    Solid_Character(obj, player, SolidChar_Sonic, 0x10, 0x10, 0x11, 100, NULL);
    CHECK(!player->status.p.f.object_stand);
    CHECK((obj->status.b & (1 << 3)) == 0);
}

void RegisterSolidTests(void) {
    RUN_TEST(Solid_StandingOnASlopeIsAtTheTablesHeight);
    RUN_TEST(Solid_StandingOnAFlatTopIsAtItsWalkingHeight);
    RUN_TEST(Solid_WalkingOffTheEdgeLetsGo);
}
