#include "test.h"

#include <string.h>

#include "Level.h"
#include "Object.h"
#include "Object/NGHZObjects.h"
#include "Object/Sonic.h"

// Neo Green Hill's arrow shooter (22) and leaves (2C), as the prototype's routines have them.

static void Reset(void) {
    memset(objects, 0, sizeof(Object) * 0x60);
    level_id = LEVEL_ID(ZoneId_ARZ, 0);
    scrpos_x.v = scrpos_y.v = 0;
    frame_count = 0;
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

static int Count(uint8_t type, uint8_t routine) {
    int n = 0;
    for (int i = 0x21; i < 0x60; i++)
        n += objects[i].type == type && objects[i].routine == routine;
    return n;
}

static void NGHZObjects_TheShooterFiresWhenSonicHasBeenNearAndGoes(void) {
    Reset();
    Object *obj = Spawn(0x22, 0, 100, 200);
    for (int i = 0; i < 100; i++)
        Obj_ArrowShooter(obj);
    CHECK_EQ(Count(0x22, 6) + Count(0x22, 8), 0); // (nobody near: no arrow)
    player->pos.l.x.f.u = 120; // within $40: it is ready
    for (int i = 0; i < 40; i++)
        Obj_ArrowShooter(obj);
    CHECK_EQ(Count(0x22, 6) + Count(0x22, 8), 0); // (and still holds its fire)
    player->pos.l.x.f.u = 0x1000; // he leaves: now it draws back and shoots
    int frames = 0;
    while (frames++ < 400 && Count(0x22, 6) + Count(0x22, 8) == 0)
        Obj_ArrowShooter(obj);
    CHECK(Count(0x22, 6) + Count(0x22, 8) >= 1);
}

static void NGHZObjects_AnArrowFliesTheWayTheShooterFaces(void) {
    Reset();
    Object *obj = Spawn(0x22, 0, 100, 200);
    Obj_ArrowShooter(obj);
    Object *arrow = &objects[0x21];
    arrow->type = 0x22;
    arrow->routine = 6;
    arrow->pos.l.x.f.u = 100;
    arrow->pos.l.y.f.u = 200;
    arrow->mappings = obj->mappings;
    Obj_ArrowShooter(arrow);
    CHECK_EQ(arrow->col_type, 0x9B); // (it hurts)
    CHECK_EQ(arrow->xsp, 0x400);
    Object *back = &objects[0x22];
    back->type = 0x22;
    back->routine = 6;
    back->status.o.f.x_flip = true;
    back->pos.l.x.f.u = 100;
    back->pos.l.y.f.u = 200;
    back->mappings = obj->mappings;
    Obj_ArrowShooter(back);
    CHECK_EQ(back->xsp, -0x400);
}

static void NGHZObjects_TheLeavesBoxHurtsNothingAndIsKeyedBySubtype(void) {
    Reset();
    static const uint8_t expected[3] = { 0xD6, 0xD4, 0xD5 }; // (the touch response's "special" kinds, the leaves' boxes)
    for (int sub = 0; sub < 3; sub++) {
        Object *obj = Spawn(0x2C, (uint8_t)sub, 100, 200);
        Obj_NGHZLeaves(obj);
        CHECK_EQ(obj->col_type, expected[sub]);
        CHECK_EQ(obj->width_pixels, 0x80);
    }
}

static void NGHZObjects_FastSonicInTheBoxMakesFourLeaves(void) {
    Reset();
    Object *obj = Spawn(0x2C, 0, 100, 200);
    Obj_NGHZLeaves(obj);
    player->pos.l.x.f.u = 100;
    player->pos.l.y.f.u = 200;
    player->xsp = 0x400; // fast
    obj->col_property = 1; // (the touch response: Sonic is in the box)
    frame_count = 0x10;     // (a frame that is a multiple of 16: Sonic's turn)
    Obj_NGHZLeaves(obj);
    CHECK_EQ(Count(0x2C, 4), 4);
}

static void NGHZObjects_SlowSonicMakesNoLeaves(void) {
    Reset();
    Object *obj = Spawn(0x2C, 0, 100, 200);
    Obj_NGHZLeaves(obj);
    player->xsp = 0x100;
    player->ysp = 0x100;
    obj->col_property = 1;
    frame_count = 0x10;
    Obj_NGHZLeaves(obj);
    CHECK_EQ(Count(0x2C, 4), 0);
}

void RegisterNGHZObjectTests(void) {
    RUN_TEST(NGHZObjects_TheShooterFiresWhenSonicHasBeenNearAndGoes);
    RUN_TEST(NGHZObjects_AnArrowFliesTheWayTheShooterFaces);
    RUN_TEST(NGHZObjects_TheLeavesBoxHurtsNothingAndIsKeyedBySubtype);
    RUN_TEST(NGHZObjects_FastSonicInTheBoxMakesFourLeaves);
    RUN_TEST(NGHZObjects_SlowSonicMakesNoLeaves);
}
