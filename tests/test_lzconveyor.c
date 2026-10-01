#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/LZConveyor.h"
#include "PaletteCycle.h"

// Object 63 (LZ conveyor platforms), LZ3's upper route: spawner subtype $85
// at ($1300, $280) creates group 5's platforms from ObjPos_LZ3pf2.
// Two bugs (2026-09):
//   1. groupid aliased the subtype byte, so once the spawner turned itself
//      into the first platform its group ID was gone -- the group's
//      "loaded" flag was never cleared on despawn and it never came back.
//   2. Platforms 2..N went into FindNextFreeObj() slots without clearing;
//      a stale nonzero routine skipped init and a garbage points pointer
//      was dereferenced at the first corner (crash).

static void RunConveyors(int frames) {
    for (int f = 0; f < frames; f++) {
        for (int i = 0; i < LEVEL_OBJECTS; i++)
            if (level_objects[i].type == ObjId_LabyrinthConvey)
                Obj_LabyrinthConvey(&level_objects[i]);
        frame_count++;
    }
}

static int CountConveyors(void) {
    int n = 0;
    for (int i = 0; i < LEVEL_OBJECTS; i++)
        if (level_objects[i].type == ObjId_LabyrinthConvey)
            n++;
    return n;
}

static void SpawnGroup5(uint8_t poison) {
    for (int i = 0; i < LEVEL_OBJECTS; i++) {
        memset(&level_objects[i], poison, sizeof(Object));
        level_objects[i].type = ObjId_Null;
    }
    memset(player, 0, sizeof(Object));
    player->routine = 2;
    player->pos.l.x.f.u = 0x1000; // well away, never standing on a platform
    player->pos.l.y.f.u = 0x700;
    level_id = LEVEL_ID(ZoneId_LZ, 2);
    memset(obj63_loaded, 0, sizeof(obj63_loaded));
    memset(f_switch, 0, sizeof(f_switch));
    f_conveyrev = false;
    scrpos_x.f.u = 0x1200;

    Object *spawner = &level_objects[0];
    memset(spawner, 0, sizeof(Object));
    spawner->type = ObjId_LabyrinthConvey;
    spawner->pos.l.x.f.u = 0x1300;
    spawner->pos.l.y.f.u = 0x280;
    spawner->scratch.u8[0] = 0x85;
    RunConveyors(1); // spawn the group
}

static void LZConveyor_GroupRespawnsAfterScrollingAway(void) {
    SpawnGroup5(0);
    int n = CountConveyors();
    CHECK(n > 1);
    CHECK(obj63_loaded[5] & 1);
    Scratch_LCon *s = (Scratch_LCon *)&level_objects[0].scratch;
    CHECK_EQ((uint8_t)s->groupid, 0x85); // survives becoming the first platform
    CHECK(s->subtype != 0x85);

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
    for (int i = 0; i < LEVEL_OBJECTS; i++) {
        Object *p = &level_objects[i];
        if (p->type != ObjId_LabyrinthConvey)
            continue;
        Scratch_LCon *s = (Scratch_LCon *)&p->scratch;
        CHECK(p->routine == 2 || p->routine == 4);
        CHECK_EQ(p->frame, 4); // platform frame
        CHECK_EQ(s->count, 4); // group 5 has 4 corners
        CHECK(s->posindex < s->count);
        CHECK(!s->reversed);
    }
    RunConveyors(600); // follows its corners without touching bad memory
    CHECK_EQ(CountConveyors(), n);
}

void RegisterLZConveyorTests(void) {
    RUN_TEST(LZConveyor_GroupRespawnsAfterScrollingAway);
    RUN_TEST(LZConveyor_PlatformsInitInDirtySlots);
}
