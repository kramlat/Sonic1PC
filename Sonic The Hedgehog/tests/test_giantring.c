#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "SpecialStage.h"

void Obj_GiantRing(Object *obj); // GiantRing.h defines its mappings arrays, so not included

// Regression test (2026-09): the giant ring appeared (and could be collected)
// no matter how many rings you had. Real object (GRing_Main): while it is off
// screen it only animates and keeps waiting in routine 0; once on screen it
// checks "6 emeralds? delete" and "at least 50 rings? then become active". The
// port advanced to the active routine in the off-screen case, and also wiped
// the renderer's on-screen bit every frame, so the check could not work.

static Object *SpawnRing(void) {
    memset(level_objects, 0, sizeof(Object) * LEVEL_OBJECTS);
    scrpos_x.f.u = 0x100;
    emeralds = 0;
    Object *r = &level_objects[0];
    r->type = ObjId_GiantRing;
    r->pos.l.x.f.u = 0x180;
    r->pos.l.y.f.u = 0x100;
    return r;
}

static void GiantRing_NeedsFiftyRings(void) {
    Object *r = SpawnRing();
    rings = 10;

    Obj_GiantRing(r); // frame it loads: not on screen yet
    CHECK_EQ(r->routine, 0);
    CHECK_EQ(r->col_type, 0);

    r->render.f.on_screen = true; // BuildSprites says it is visible
    for (int i = 0; i < 5; i++) {
        Obj_GiantRing(r);
        CHECK_EQ(r->routine, 0); // still waiting
        CHECK_EQ(r->col_type, 0); // not collectable
        CHECK(r->render.f.on_screen); // the flag must survive the main routine
    }

    rings = 49;
    Obj_GiantRing(r);
    CHECK_EQ(r->routine, 0);

    rings = 50;
    Obj_GiantRing(r);
    CHECK_EQ(r->routine, 2); // now active
    CHECK_EQ(r->col_type, 0x52);
}

static void GiantRing_DeletedWithAllEmeralds(void) {
    Object *r = SpawnRing();
    rings = 99;
    emeralds = 6;
    Obj_GiantRing(r);
    CHECK_EQ(r->type, ObjId_GiantRing); // off screen on the first frame: nothing decided yet
    r->render.f.on_screen = true;
    Obj_GiantRing(r);
    CHECK_EQ(r->type, ObjId_Null);
}

void RegisterGiantRingTests(void) {
    RUN_TEST(GiantRing_NeedsFiftyRings);
    RUN_TEST(GiantRing_DeletedWithAllEmeralds);
}
