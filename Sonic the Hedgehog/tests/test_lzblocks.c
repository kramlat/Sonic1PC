#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelCollision.h"
#include "LevelDraw.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/LZBlocks.h"
#include "SstLoad.h"

// Labyrinth Zone's pulley platforms (object 61, subtype 13: a 64x24 platform that rises once stood on, until it meets the ceiling).

static Sst *Spawn(int16_t x, int16_t y, uint8_t subtype) {
    Sst *o = Sst_Spawn(ObjId_LabyrinthBlock, x, y);
    o->subtype = subtype;
    return o;
}

static void LZBlocks_PlatformsRiseToTheCeilingAndStopThere(void) {
    level_id = LEVEL_ID(ZoneId_LZ, 0);
    LevelDataLoad();
    ColIndexLoad();
    Sst_Reset();
    memset(player, 0, sizeof(Sst));
    player->routine = 2;
    player->x_pos.f.u = 0xA60;
    player->y_pos.f.u = 0x200;
    scrpos_x.f.u = 0x980;
    scrpos_y.f.u = 0x2C0;
    Sst_Reset();
    static const SstObjectIndex index_ = {sst_object_codes, 0x100};
    SstLoadCamera camera = {0x980, 0x2C0, false, 0xFFF};
    SstLoad_Init(&index_, NULL, false, &camera);

    Sst *a = Spawn(0xA20, 0x30C, 0x14); // already rising (type 4)
    Sst *b = Spawn(0xAA0, 0x348, 0x14);
    for (int frame = 0; frame < 400; frame++)
        ExecuteObjects();
    CHECK_EQ(a->subtype & 0xF, 0); // stopped at the ceiling
    CHECK_EQ(b->subtype & 0xF, 0);
}

void RegisterLZBlocksTests(void) {
    RUN_TEST(LZBlocks_PlatformsRiseToTheCeilingAndStopThere);
}
