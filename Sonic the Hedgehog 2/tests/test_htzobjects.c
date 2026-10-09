#include "test.h"

#include <string.h>

#include "Level.h"
#include "Object.h"
#include "Object/CPZObjects.h"
#include "Object/HTZObjects.h"
#include "Object/Platform.h"
#include "Object/Sonic.h"

// Hill Top's lift (16), the valve barrier and rock (2D, 32, the objects Chemical Plant shares) and the platform (18) with its size field and solid kind.

static void Reset(void) {
    memset(objects, 0, sizeof(Object) * 0x60);
    level_id = LEVEL_ID(ZoneId_HTZ, 0);
    scrpos_x.v = scrpos_y.v = 0;
    limit_btm2 = 0x720;
    player->routine = 2;
    player->pos.l.x.f.u = 0x1000;
    player->pos.l.y.f.u = 0x1000;
}

static Object *Spawn(uint8_t type, uint8_t subtype, int16_t x, int16_t y) {
    Object *obj = &objects[0x20];
    memset(obj, 0, sizeof(Object));
    obj->type = type;
    obj->scratch.u8[0] = subtype;
    obj->pos.l.x.f.u = x;
    obj->pos.l.y.f.u = y;
    return obj;
}

// The lift waits to be stood on, slides (to the right, or the left if flipped) for its subtype times eight frames, then stops, leaves a pole and falls
static void HTZObjects_ALiftSlidesForItsSubtypeAndThenFalls(void) {
    Reset();
    Object *obj = Spawn(0x16, 0x14, 100, 200);
    Obj_HTZLift(obj);
    for (int i = 0; i < 10; i++)
        Obj_HTZLift(obj);
    CHECK_EQ(obj->pos.l.x.f.u, 100); // (it waits until it is stood on)
    obj->status.b |= 1 << 3;
    Obj_HTZLift(obj);
    CHECK_EQ(obj->xsp, 0x200);
    CHECK_EQ(obj->ysp, 0x100);
    for (int i = 0; i < 0x14 * 8; i++)
        Obj_HTZLift(obj);
    CHECK_EQ(obj->frame, 2); // (stopped, with its pole gone)
    CHECK_EQ(obj->xsp, 0);
    int poles = 0;
    for (int i = 0x21; i < 0x60; i++)
        poles += objects[i].type == 0x1C && objects[i].scratch.u8[0] == 6;
    CHECK_EQ(poles, 1);
    const int16_t y = obj->pos.l.y.f.u;
    for (int i = 0; i < 10; i++)
        Obj_HTZLift(obj);
    CHECK(obj->pos.l.y.f.u > y); // falling
}

static void HTZObjects_AFlippedLiftSlidesLeft(void) {
    Reset();
    Object *obj = Spawn(0x16, 0x14, 100, 200);
    obj->status.o.f.x_flip = true;
    Obj_HTZLift(obj);
    obj->status.b |= 1 << 3;
    Obj_HTZLift(obj);
    CHECK_EQ(obj->xsp, -0x200);
}

static void HTZObjects_TheValveBarrierHasItsOwnArtAndWidth(void) {
    Reset();
    Object *obj = Spawn(0x2D, 0, 400, 300);
    Obj_CPZBarrier(obj);
    CHECK_EQ(obj->tile & 0x7FF, 0x426);
    CHECK_EQ(obj->width_pixels, 8);
    level_id = LEVEL_ID(ZoneId_CPZ, 0);
    Object *cpz = Spawn(0x2D, 0, 400, 300);
    Obj_CPZBarrier(cpz);
    CHECK_EQ(cpz->tile & 0x7FF, 0x394);
    CHECK_EQ(cpz->width_pixels, 0xC);
}

static void HTZObjects_TheRockIsWiderAndBreaksInSix(void) {
    Reset();
    Object *obj = Spawn(0x32, 0, 300, 300);
    Obj_TubeCover(obj);
    CHECK_EQ(obj->tile & 0x7FF, 0x3B2);
    CHECK_EQ(obj->width_pixels, 0x18);
    level_id = LEVEL_ID(ZoneId_CPZ, 1);
    Object *cover = Spawn(0x32, 0, 300, 300);
    Obj_TubeCover(cover);
    CHECK_EQ(cover->width_pixels, 0x10);
}

static void HTZObjects_ThePlatformSizeIsThreeBitsAndBitSevenIsTheSolidKind(void) {
    Reset();
    // sizes: widths $20, $20, $20, $40, $30 by (subtype >> 4) & 7
    Object *a = Spawn(0x18, 0x30, 300, 300);
    Obj_BasicPlatform(a);
    CHECK_EQ(a->width_pixels, 0x40);
    Object *b = Spawn(0x18, 0x40, 300, 300);
    Obj_BasicPlatform(b);
    CHECK_EQ(b->width_pixels, 0x30);
    Object *solid = Spawn(0x18, 0x9B, 300, 300); // bit 7: the solid block, size (9B >> 4) & 7 = 1
    Obj_BasicPlatform(solid);
    CHECK_EQ(solid->width_pixels, 0x20);
    CHECK_EQ(solid->y_rad, 0x30); // (Neo Green Hill's is $28)
    level_id = LEVEL_ID(ZoneId_ARZ, 0);
    Object *arz = Spawn(0x18, 0x9B, 300, 300);
    Obj_BasicPlatform(arz);
    CHECK_EQ(arz->y_rad, 0x28);
}

void RegisterHTZObjectTests(void) {
    RUN_TEST(HTZObjects_ALiftSlidesForItsSubtypeAndThenFalls);
    RUN_TEST(HTZObjects_AFlippedLiftSlidesLeft);
    RUN_TEST(HTZObjects_TheValveBarrierHasItsOwnArtAndWidth);
    RUN_TEST(HTZObjects_TheRockIsWiderAndBreaksInSix);
    RUN_TEST(HTZObjects_ThePlatformSizeIsThreeBitsAndBitSevenIsTheSolidKind);
}
