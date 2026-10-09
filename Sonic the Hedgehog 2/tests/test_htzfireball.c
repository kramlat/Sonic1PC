#include "test.h"

#include <string.h>

#include "Level.h"
#include "Object.h"
#include "Object/HTZObjects.h"

// Hill Top's fireball (Obj_0x20, the lava bubble): subtype $44 makes balls that go up and out at $200, and waits $40 frames between throws.

static Object *Bubble(uint8_t subtype) {
    memset(objects, 0, sizeof(Object) * 0x60);
    scrpos_x.v = scrpos_y.v = 0;
    limit_btm2 = 0x720;
    Object *obj = &objects[0x20];
    obj->type = 0x20;
    obj->scratch.u8[0] = subtype;
    obj->pos.l.x.f.u = 100;
    obj->pos.l.y.f.u = 200;
    return obj;
}

static void HTZFireball_SubtypeSetsSpeedAndWait(void) {
    Object *obj = Bubble(0x44);
    Obj_HTZFireball(obj);
    CHECK_EQ(obj->xsp, -0x200);
    CHECK_EQ(obj->ysp, -0x200);
    CHECK_EQ(obj->scratch.s16[(0x32 - 0x28) / 2], 0x40); // the wait
    CHECK_EQ(obj->scratch.s16[(0x34 - 0x28) / 2], 0x40);
    CHECK_EQ(obj->tile & 0x7FF, 0x416);
}

static void HTZFireball_ThrowsTwoBallsTheOtherWay(void) {
    Object *obj = Bubble(0x44);
    int frames = 0;
    int balls = 0, left = 0, right = 0;
    while (frames++ < 400 && balls == 0) {
        Obj_HTZFireball(obj);
        for (int i = 0x21; i < 0x60; i++)
            if (objects[i].type == 0x20 && objects[i].routine == 8) {
                balls++;
                if (objects[i].xsp < 0)
                    left++;
                else
                    right++;
            }
    }
    CHECK_EQ(balls, 2);
    CHECK_EQ(left, 1);
    CHECK_EQ(right, 1);
    for (int i = 0x21; i < 0x60; i++)
        if (objects[i].type == 0x20) {
            CHECK_EQ(objects[i].col_type, 0x8B); // (they hurt)
            CHECK_EQ(objects[i].ysp, -0x200);
        }
}

void RegisterHTZFireballTests(void) {
    RUN_TEST(HTZFireball_SubtypeSetsSpeedAndWait);
    RUN_TEST(HTZFireball_ThrowsTwoBallsTheOtherWay);
}
