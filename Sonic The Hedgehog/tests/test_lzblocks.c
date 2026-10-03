#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelCollision.h"
#include "LevelDraw.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/LZBlocks.h"

// Labyrinth Zone's pulley platforms (object 61, subtype 13: a 64x24 platform that rises once stood on, until it meets the ceiling).

static Object *Spawn(int slot, int16_t x, int16_t y, uint8_t subtype) {
    Object *o = &objects[slot];
    memset(o, 0, sizeof(Object));
    o->type = ObjId_LabyrinthBlock;
    o->pos.l.x.f.u = x;
    o->pos.l.y.f.u = y;
    o->scratch.u8[0] = subtype;
    return o;
}

static void LZBlocks_PlatformsRiseToTheCeilingAndStopThere(void) {
    level_id = LEVEL_ID(ZoneId_LZ, 0);
    LevelDataLoad();
    ColIndexLoad();
    memset(objects, 0, sizeof(objects));
    memset(player, 0, sizeof(Object));
    player->routine = 2;
    player->pos.l.x.f.u = 0xA60;
    player->pos.l.y.f.u = 0x200;
    scrpos_x.f.u = 0x980;
    scrpos_y.f.u = 0x2C0;

    Object *a = Spawn(40, 0xA20, 0x30C, 0x14); // already rising (type 4)
    Object *b = Spawn(41, 0xAA0, 0x348, 0x14);
    for (int frame = 0; frame < 400; frame++) {
        if (frame % 100 == 0)
            printf("\n    [diag] frame %3d: A y=%04X ysp=%d sub=%02X   B y=%04X ysp=%d sub=%02X", frame, (uint16_t)a->pos.l.y.f.u,
                   a->ysp, a->scratch.u8[0], (uint16_t)b->pos.l.y.f.u, b->ysp, b->scratch.u8[0]);
        ExecuteObjects();
    }
    printf("\n    [diag] end: A y=%04X sub=%02X   B y=%04X sub=%02X", (uint16_t)a->pos.l.y.f.u, a->scratch.u8[0], (uint16_t)b->pos.l.y.f.u,
           b->scratch.u8[0]);
    CHECK_EQ(a->scratch.u8[0] & 0xF, 0); // stopped at the ceiling
    CHECK_EQ(b->scratch.u8[0] & 0xF, 0);
}

void RegisterLZBlocksTests(void) {
    RUN_TEST(LZBlocks_PlatformsRiseToTheCeilingAndStopThere);
}
