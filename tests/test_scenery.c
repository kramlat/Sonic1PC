#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
void Obj_Scenery(Object *obj); // Scenery.h pulls in mapping arrays

// Object 1C uses `ori.b #sprite_cam_field,obRender`, so the X/Y flip bits
// from the object layout survive init and the scenery faces the way it was placed.
static void Scenery_KeepsPlacedFlip(void) {
    memset(level_objects, 0, sizeof(Object) * LEVEL_OBJECTS);
    Object *o = &level_objects[2];
    o->type = ObjId_Scenery;
    o->scratch.u8[0] = 3;
    o->render.f.x_flip = true;
    o->render.f.y_flip = true;
    o->pos.l.x.f.u = 0x100;
    scrpos_x.f.u = 0;
    Obj_Scenery(o);
    CHECK(o->render.f.x_flip);
    CHECK(o->render.f.y_flip);
    CHECK(o->render.f.align_fg);
}

void RegisterSceneryTests(void) {
    RUN_TEST(Scenery_KeepsPlacedFlip);
}
