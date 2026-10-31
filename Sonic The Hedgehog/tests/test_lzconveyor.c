#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/LZConveyor.h"
#include "SstLoad.h"
#include "PaletteCycle.h"

// Object 63 (LZ conveyor platforms), LZ3's upper route: spawner subtype $85
// at ($1300, $280) creates group 5's platforms from ObjPos_LZ3pf2.
// Two bugs (2026-09):
//   1. groupid aliased the subtype byte, so once the spawner turned itself
//      into the first platform its group ID was gone -- the group's
//      "loaded" flag was never cleared on despawn and it never came back.
//   2. Platforms 2..N went into free slots without clearing;
//      a stale nonzero routine skipped init and a garbage points pointer
//      was dereferenced at the first corner (crash).

static void Camera(void) {
    static const SstObjectIndex index_ = {sst_object_codes, 0x100};
    (void)index_;
    SstLoadCamera camera = {scrpos_x.f.u, 0, false, 0xFFF};
    SstLoad_Update(&camera);
}

static void RunConveyors(int frames) {
    for (int f = 0; f < frames; f++) {
        Camera();
        Sst_Process(false);
        frame_count++;
    }
}

static int CountConveyors(void) {
    int n = 0;
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code == sst_object_codes[ObjId_LabyrinthConvey])
            n++;
    return n;
}

static void ClearTable(uint8_t poison) {
    Sst_Reset();
    for (int i = SST_FIRST_DYNAMIC; i < SST_COUNT; i++) {
        memset(&sst[i], poison, sizeof(Sst));
        sst[i].code = NULL;
    }
}

static Sst *spawner_slot;

static void SpawnGroup5(uint8_t poison) {
    ClearTable(poison);
    memset(player, 0, sizeof(Sst));
    player->routine = 2;
    player->x_pos.f.u = 0x1000; // well away, never standing on a platform
    player->y_pos.f.u = 0x700;
    level_id = LEVEL_ID(ZoneId_LZ, 2);
    memset(obj63_loaded, 0, sizeof(obj63_loaded));
    memset(f_switch, 0, sizeof(f_switch));
    f_conveyrev = false;
    scrpos_x.f.u = 0x1200;

    static const SstObjectIndex index_ = {sst_object_codes, 0x100};
    SstLoadCamera camera = {0x1200, 0, false, 0xFFF};
    SstLoad_Init(&index_, NULL, false, &camera);
    Sst *spawner = Sst_Spawn(ObjId_LabyrinthConvey, 0x1300, 0x280);
    SstCode code = spawner->code;
    memset(spawner, 0, sizeof(Sst)); // (the loader gives a clean slot: the table's free slots are cleared by Sst_Delete)
    spawner->code = code;
    spawner->x_pos.f.u = 0x1300;
    spawner->y_pos.f.u = 0x280;
    spawner_slot = spawner;
    spawner->subtype = 0x85;
    RunConveyors(1); // spawn the group
}

static void LZConveyor_GroupRespawnsAfterScrollingAway(void) {
    SpawnGroup5(0);
    int n = CountConveyors();
    CHECK(n > 1);
    CHECK(obj63_loaded[5] & 1);
    Own_LCon *s = (Own_LCon *)spawner_slot->own.u8;
    CHECK_EQ((uint8_t)s->groupid, 0x85); // survives becoming the first platform
    CHECK(spawner_slot->subtype != 0x85);

    RunConveyors(10);
    scrpos_x.f.u = 0x2000; // far away -- every platform goes out of range
    RunConveyors(1);
    CHECK_EQ(CountConveyors(), 0);
    CHECK_EQ(obj63_loaded[5] & 1, 0); // cleared, so the group can reload
}

static void LZConveyor_PlatformsInitInDirtySlots(void) {
    SpawnGroup5(0xAB); // every free slot full of junk, including routine
    int n = CountConveyors();
    CHECK(n > 1);
    RunConveyors(1); // each platform runs its init
    for (int i = 0; i < SST_COUNT; i++) {
        Sst *p = &sst[i];
        if (p->code != sst_object_codes[ObjId_LabyrinthConvey])
            continue;
        Own_LCon *s = (Own_LCon *)p->own.u8;
        CHECK(p->routine == 2 || p->routine == 4);
        CHECK_EQ(p->mapping_frame, 4); // platform frame
        CHECK_EQ(s->count, 4); // group 5 has 4 corners
        CHECK(s->posindex < s->count);
        CHECK(!s->reversed);
    }
    RunConveyors(600); // follows its corners without touching bad memory
    CHECK_EQ(CountConveyors(), n);
}

// A group spawner that sits late in the object pool (the slot is the first free one when the level object loader finds it, so this
// depends on what else was loaded) must still get all of its platforms: they went into slots after the spawner only, and the ones
// that did not fit were silently dropped, leaving a rope loop with one or two platforms instead of the full set.
static void LZConveyor_SpawnerLateInThePoolStillMakesTheWholeGroup(void) {
    ClearTable(0);
    memset(player, 0, sizeof(Sst));
    player->routine = 2;
    player->x_pos.f.u = 0x100;
    player->y_pos.f.u = 0x100;
    level_id = LEVEL_ID(ZoneId_LZ, 1);
    memset(obj63_loaded, 0, sizeof(obj63_loaded));
    memset(f_switch, 0, sizeof(f_switch));
    f_conveyrev = false;
    scrpos_x.f.u = 0xC80;

    static const SstObjectIndex index_ = {sst_object_codes, 0x100};
    SstLoadCamera camera = {0xC80, 0, false, 0xFFF};
    SstLoad_Init(&index_, NULL, false, &camera);
    Sst *spawner = &sst[SST_COUNT - 3];
    spawner->code = sst_object_codes[ObjId_LabyrinthConvey];
    spawner->x_pos.f.u = 0xD22;
    spawner->y_pos.f.u = 0x483;
    spawner->subtype = 0x82; // group 2: 8 platforms
    spawner->code(spawner);
    CHECK_EQ(CountConveyors(), 8);
}

void RegisterLZConveyorTests(void) {
    RUN_TEST(LZConveyor_SpawnerLateInThePoolStillMakesTheWholeGroup);
    RUN_TEST(LZConveyor_GroupRespawnsAfterScrollingAway);
    RUN_TEST(LZConveyor_PlatformsInitInDirtySlots);
}
