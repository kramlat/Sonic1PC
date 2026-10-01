#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelCollision.h"
#include "LevelDraw.h"
#include "LevelScroll.h"
#include "Object.h"

void Obj_LabyrinthBlock(Object *obj);

// Regression test (2026-09): ObjHitCeiling/ObjHitWallRight/ObjHitWallLeft
// probed the top-solid bit instead of the left/right/bottom bit P128
// hardcodes ($D). LZ1's rising platform near the first tunnel, nudged 4px
// down by Sonic standing on it (y=$34C), got a bogus -32 "ceiling hit" on its
// very first rising frame and was shoved into the floor, taking Sonic with it.

static Object *SpawnBlock(int16_t x, int16_t y, uint8_t subtype) {
    memset(level_objects, 0, sizeof(Object) * LEVEL_OBJECTS);
    memset(player, 0, sizeof(Object));
    player->routine = 2;
    player->pos.l.x.f.u = 0x900;
    player->pos.l.y.f.u = 0x100;
    scrpos_x.f.u = x - 160;
    scrpos_y.f.u = y - 112;

    Object *b = &level_objects[0];
    b->type = ObjId_LabyrinthBlock;
    b->pos.l.x.f.u = x;
    b->pos.l.y.f.u = y;
    b->scratch.u8[0] = subtype;
    Obj_LabyrinthBlock(b); // init
    return b;
}

static void LZ1Rise_NoBogusCeilingHit(void) {
    level_id = LEVEL_ID(ZoneId_LZ, 0);
    LevelDataLoad();
    ColIndexLoad();

    // Every rising platform in LZ1, at its resting Y and nudged down by up to
    // 4px (Sonic standing on it), must see free space above it.
    static const struct { int16_t x, y; } plat[] = {
        { 0xA20, 0x30C }, { 0xAA0, 0x348 }, { 0xC20, 0x450 }, { 0xC80, 0x44C },
    };
    for (size_t i = 0; i < sizeof(plat) / sizeof(plat[0]); i++) {
        for (int nudge = 0; nudge <= 4; nudge++) {
            Object probe;
            memset(&probe, 0, sizeof(probe));
            probe.pos.l.x.f.u = plat[i].x;
            probe.pos.l.y.f.u = (int16_t)(plat[i].y + nudge);
            probe.y_rad = 24 / 2;
            CHECK(ObjHitCeiling(&probe) >= 0);
        }
    }
}

static void LZ1Rise_PlatformRisesFromNudgedPosition(void) {
    level_id = LEVEL_ID(ZoneId_LZ, 0);
    LevelDataLoad();
    ColIndexLoad();

    Object *b = SpawnBlock(0xAA0, 0x34C, 0x14); // nudged down, straight into the rise
    for (int f = 0; f < 60; f++)
        Obj_LabyrinthBlock(b);
    CHECK(b->pos.l.y.f.u < 0x34C); // went up, never down
    CHECK((b->scratch.u8[0] & 0xF) == 4); // still rising, not stopped by a fake ceiling
}

void RegisterObjCollisionTests(void) {
    RUN_TEST(LZ1Rise_NoBogusCeilingHit);
    RUN_TEST(LZ1Rise_PlatformRisesFromNudgedPosition);
}

// Leaving LZ for the title screen (or a special stage) must turn the water
// palette split off -- otherwise the title draws with the water palette
// below the old water line.
void VDPDisableWaterSplit(void);
extern int16_t hbla_counter;

static void Water_TitleScreenDisablesSplit(void) {
    hbla_counter = 100; // LZ water line partway down the screen
    wtr_state = 1;
    hblank_pal = true;
    doupdatesinhblank = true;

    VDPDisableWaterSplit();

    CHECK_EQ(hbla_counter, 223);
    CHECK_EQ(wtr_state, 0);
    CHECK(!hblank_pal);
    CHECK(!doupdatesinhblank);
}

void RegisterWaterSplitTests(void) {
    RUN_TEST(Water_TitleScreenDisablesSplit);
}
