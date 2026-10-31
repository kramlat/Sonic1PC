#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "SpecialStage.h"

#include "Object/GiantRing.h"
#include "SstLoad.h"

// Regression test (2026-09): the giant ring appeared (and could be collected)
// no matter how many rings you had. Real object (GRing_Main): while it is off
// screen it only animates and keeps waiting in routine 0; once on screen it
// checks "6 emeralds? delete" and "at least 50 rings? then become active". The
// port advanced to the active routine in the off-screen case, and also wiped
// the renderer's on-screen bit every frame, so the check could not work.

static Sst *SpawnRing(void) {
    Sst_Reset();
    scrpos_x.f.u = 0x100;
    emeralds = 0;
    static const SstObjectIndex index_ = {sst_object_codes, 0x100};
    SstLoadCamera camera = {0x100, 0, false, 0xFFF};
    SstLoad_Init(&index_, NULL, false, &camera);
    return Sst_Spawn(ObjId_GiantRing, 0x180, 0x100);
}

static void GiantRing_NeedsFiftyRings(void) {
    Sst *r = SpawnRing();
    rings = 10;

    Obj_GiantRing(r); // frame it loads: not on screen yet
    CHECK_EQ(r->routine, 0);
    CHECK_EQ(r->collision_flags, 0);

    r->render_flags |= SST_RENDER_ON_SCREEN; // BuildSprites says it is visible
    for (int i = 0; i < 5; i++) {
        Obj_GiantRing(r);
        CHECK_EQ(r->routine, 0); // still waiting
        CHECK_EQ(r->collision_flags, 0); // not collectable
        CHECK(r->render_flags & SST_RENDER_ON_SCREEN); // the flag must survive the main routine
    }

    rings = 49;
    Obj_GiantRing(r);
    CHECK_EQ(r->routine, 0);

    rings = 50;
    Obj_GiantRing(r);
    CHECK_EQ(r->routine, 2); // now active
    CHECK_EQ(r->collision_flags, 0x52);
}

static void GiantRing_DeletedWithAllEmeralds(void) {
    Sst *r = SpawnRing();
    rings = 99;
    emeralds = 6;
    Obj_GiantRing(r);
    CHECK(r->code != NULL); // off screen on the first frame: nothing decided yet
    r->render_flags |= SST_RENDER_ON_SCREEN;
    Obj_GiantRing(r);
    CHECK(r->code == NULL);
}

void RegisterGiantRingTests(void) {
    RUN_TEST(GiantRing_NeedsFiftyRings);
    RUN_TEST(GiantRing_DeletedWithAllEmeralds);
}
