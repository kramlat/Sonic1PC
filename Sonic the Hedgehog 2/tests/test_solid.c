#include "test.h"

#include <string.h>

#include "Level.h"
#include "Object.h"
#include "SstLoad.h"
#include "Object/Sonic.h"
#include "Solid.h"

// Solid_Character, the prototype's SolidObject family. A character standing on a sloped solid (a lever spring, a diagonal spring) is put where the slope table says its top is: at the object's y minus the
// table's height at his column. The first version of this added the table's first entry instead of taking it away and sank him by twice that entry (16 pixels into a lever spring).

static Sst *Reset(void) {
    Sst_Reset();
    return Sst_Spawn(0x15, 0, 0); // a dynamic slot
}

static void Solid_StandingOnASlopeIsAtTheTablesHeight(void) {
    static const int8_t slope[0x20] = { 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8 };
    Sst *obj = Reset();
    obj->x_pos.f.u = 100;
    obj->y_pos.f.u = 200;
    obj->status = 1 << 3; // Sonic stands on it
    player->x_pos.f.u = 100;
    player->y_pos.f.u = 0;
    player->y_radius = 19;
    player->status |= SST_PSTATUS_OBJECT_STAND;
    player->status &= (uint8_t)~SST_PSTATUS_IN_AIR;
    player->routine = 2;

    Solid_Character(obj, player, SolidChar_Sonic, 0x10, 0x10, 0x11, 100, slope);
    CHECK_EQ(player->y_pos.f.u, 200 - 8 - 19); // (the table's value 8 above the object's y, then his own radius: he stands on it, not 16 pixels in it)
}

static void Solid_StandingOnAFlatTopIsAtItsWalkingHeight(void) {
    Sst *obj = Reset();
    obj->x_pos.f.u = 100;
    obj->y_pos.f.u = 200;
    obj->status = 1 << 3;
    player->x_pos.f.u = 100;
    player->y_radius = 19;
    player->status |= SST_PSTATUS_OBJECT_STAND;
    player->routine = 2;

    Solid_Character(obj, player, SolidChar_Sonic, 0x10, 0x10, 0x11, 100, NULL);
    CHECK_EQ(player->y_pos.f.u, 200 - 0x11 - 19);
}

static void Solid_WalkingOffTheEdgeLetsGo(void) {
    Sst *obj = Reset();
    obj->x_pos.f.u = 100;
    obj->y_pos.f.u = 200;
    obj->status = 1 << 3;
    player->x_pos.f.u = 100 + 0x40; // far past the edge
    player->y_radius = 19;
    player->status |= SST_PSTATUS_OBJECT_STAND;
    player->routine = 2;

    Solid_Character(obj, player, SolidChar_Sonic, 0x10, 0x10, 0x11, 100, NULL);
    CHECK(!((player->status & SST_PSTATUS_OBJECT_STAND) != 0));
    CHECK((obj->status & (1 << 3)) == 0);
}

void RegisterSolidTests(void) {
    RUN_TEST(Solid_StandingOnASlopeIsAtTheTablesHeight);
    RUN_TEST(Solid_StandingOnAFlatTopIsAtItsWalkingHeight);
    RUN_TEST(Solid_WalkingOffTheEdgeLetsGo);
}
