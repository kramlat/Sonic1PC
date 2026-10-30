#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/Scenery.h"
#include "SstLoad.h"

// Object 1C uses `ori.b #sprite_cam_field,obRender`, so the X/Y flip bits
// from the object layout survive init and the scenery faces the way it was placed.
static void Scenery_KeepsPlacedFlip(void) {
    Sst_Reset();
    static const SstObjectIndex index_ = {sst_object_codes, 0x100};
    SstLoadCamera camera = {0, 0, false, 0xFFF};
    SstLoad_Init(&index_, NULL, false, &camera);
    scrpos_x.f.u = 0;
    Sst *o = Sst_Spawn(ObjId_Scenery, 0x100, 0x100);
    o->subtype = 3;
    o->render_flags = SST_RENDER_X_FLIP | SST_RENDER_Y_FLIP;
    o->code(o);
    CHECK(o->render_flags & SST_RENDER_X_FLIP);
    CHECK(o->render_flags & SST_RENDER_Y_FLIP);
    CHECK(o->render_flags & SST_RENDER_LEVEL);
}

void RegisterSceneryTests(void) {
    RUN_TEST(Scenery_KeepsPlacedFlip);
}
