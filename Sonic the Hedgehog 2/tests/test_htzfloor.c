#include "test.h"

#include <string.h>

#include "Level.h"
#include "Object.h"
#include "SstLoad.h"
#include "Object/HTZObjects.h"
#include "Object/Sonic.h"

// Hill Top's breakable floor (Obj_0x2F): it breaks for a roller standing on it who walks the secondary collision path (the solid bits $E/$F that the path swappers give each character), or with subtype
// bit 7 for any roller; whoever stands on it and does not break it is put back on the first path ($C/$D).

static Sst *Floor(uint8_t subtype) {
    scrpos_x.v = scrpos_y.v = 0; // (the camera at the level's start: the floor is near it, so it is not forgotten for being out of range)
    Sst *obj = &sst[0x20];
    obj->code = sst_object_codes[0x2F];
    obj->subtype = subtype;
    obj->x_pos.f.u = 100;
    obj->y_pos.f.u = 200;
    obj->render_flags |= SST_RENDER_ON_SCREEN;
    obj->status = 1 << 3; // Sonic is standing on it
    Obj_HTZBreakFloor(obj); // (its first call sets it up)
    obj->status = 1 << 3;
    obj->render_flags |= SST_RENDER_ON_SCREEN;
    return obj;
}

static void Roller(uint8_t top_solid_bit) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)player->own.u8;
    player->x_pos.f.u = 100;
    player->y_pos.f.u = 200 - 0x25 - 19;
    player->y_radius = 14;
    player->routine = 2;
    player->anim = SonAnimId_Roll;
    player->status = 0;
    player->status |= SST_PSTATUS_OBJECT_STAND;
    player->status |= SST_PSTATUS_IN_BALL;
    scratch->top_solid_bit = top_solid_bit;
    scratch->lrb_solid_bit = (uint8_t)(top_solid_bit + 1);
}

static void HTZFloor_ABallOnTheSecondPathBreaksIt(void) {
    Sst *obj = Floor(0);
    Roller(0xE);
    Obj_HTZBreakFloor(obj);
    CHECK_EQ(obj->routine, 4); // it has become its first piece
    CHECK(((player->status & SST_PSTATUS_IN_AIR) != 0));
    CHECK(!((player->status & SST_PSTATUS_OBJECT_STAND) != 0));
}

static void HTZFloor_ABallOnTheFirstPathDoesNotBreakIt(void) {
    Sst *obj = Floor(0);
    Roller(0xC);
    Obj_HTZBreakFloor(obj);
    CHECK_EQ(obj->routine, 2);
    CHECK(!((player->status & SST_PSTATUS_IN_AIR) != 0));
    CHECK_EQ(((Scratch_Sonic *)player->own.u8)->top_solid_bit, 0xC);
}

static void HTZFloor_SubtypeBitSevenBreaksItForAnyRoller(void) {
    Sst *obj = Floor(0x80);
    Roller(0xC);
    Obj_HTZBreakFloor(obj);
    CHECK_EQ(obj->routine, 4);
}

static void HTZFloor_AWalkerOnTheSecondPathIsPutBackOnTheFirst(void) {
    Sst *obj = Floor(0);
    Roller(0xE);
    player->anim = SonAnimId_Walk;
    player->status &= (uint8_t)~SST_PSTATUS_IN_BALL;
    Obj_HTZBreakFloor(obj);
    CHECK_EQ(obj->routine, 2);
    CHECK_EQ(((Scratch_Sonic *)player->own.u8)->top_solid_bit, 0xC);
    CHECK_EQ(((Scratch_Sonic *)player->own.u8)->lrb_solid_bit, 0xD);
}

// The see-saw's ball is the Sol badnik (the prototype's Obj14_Ball_Init: tile $3DE, over the art the zone's art list loads at $7BC0), not drawn from the see-saw's own tiles ($3C6)
static void HTZSeesaw_TheBallIsTheBadniksArt(void) {
    scrpos_x.v = scrpos_y.v = 0;
    Sst *seesaw = &sst[0x20];
    seesaw->code = sst_object_codes[0x14];
    seesaw->subtype = 0;
    seesaw->x_pos.f.u = 100;
    seesaw->y_pos.f.u = 200;
    Obj_HTZSeesaw(seesaw);
    CHECK_EQ(seesaw->art_tile & 0x7FF, 0x3C6);
    Sst *ball = NULL;
    for (int i = 0x21; i < 0x40; i++)
        if (sst[i].code == sst_object_codes[0x14])
            ball = &sst[i];
    CHECK(ball != NULL);
    if (ball != NULL) {
        Obj_HTZSeesaw(ball); // (its first call sets it up)
        CHECK_EQ(ball->art_tile & 0x7FF, 0x3DE);
    }
}

void RegisterHTZFloorTests(void) {
    RUN_TEST(HTZSeesaw_TheBallIsTheBadniksArt);
    RUN_TEST(HTZFloor_ABallOnTheSecondPathBreaksIt);
    RUN_TEST(HTZFloor_ABallOnTheFirstPathDoesNotBreakIt);
    RUN_TEST(HTZFloor_SubtypeBitSevenBreaksItForAnyRoller);
    RUN_TEST(HTZFloor_AWalkerOnTheSecondPathIsPutBackOnTheFirst);
}
