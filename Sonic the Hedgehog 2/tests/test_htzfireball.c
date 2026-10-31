#include "test.h"

#include <string.h>

#include "Level.h"
#include "Object.h"
#include "SstLoad.h"
#include "Object/HTZObjects.h"

// Hill Top's fireball (Obj_0x20, the lava bubble): subtype $44 makes balls that go up and out at $200, and waits $40 frames between throws.

static Sst *Bubble(uint8_t subtype) {
    scrpos_x.v = scrpos_y.v = 0;
    limit_btm2 = 0x720;
    Sst *obj = &sst[0x20];
    obj->code = sst_object_codes[0x20];
    obj->subtype = subtype;
    obj->x_pos.f.u = 100;
    obj->y_pos.f.u = 200;
    return obj;
}

static void HTZFireball_SubtypeSetsSpeedAndWait(void) {
    Sst *obj = Bubble(0x44);
    Obj_HTZFireball(obj);
    CHECK_EQ(obj->x_vel, -0x200);
    CHECK_EQ(obj->y_vel, -0x200);
    CHECK_EQ(obj->own.s16[(0x32 - 0x28) / 2], 0x40); // the wait
    CHECK_EQ(obj->own.s16[(0x34 - 0x28) / 2], 0x40);
    CHECK_EQ(obj->art_tile & 0x7FF, 0x416);
}

static void HTZFireball_ThrowsTwoBallsTheOtherWay(void) {
    Sst *obj = Bubble(0x44);
    int frames = 0;
    int balls = 0, left = 0, right = 0;
    while (frames++ < 400 && balls == 0) {
        Obj_HTZFireball(obj);
        for (int i = 0x21; i < 0x60; i++)
            if (sst[i].code == sst_object_codes[0x20] && sst[i].routine == 8) {
                balls++;
                if (sst[i].x_vel < 0)
                    left++;
                else
                    right++;
            }
    }
    CHECK_EQ(balls, 2);
    CHECK_EQ(left, 1);
    CHECK_EQ(right, 1);
    for (int i = 0x21; i < 0x60; i++)
        if (sst[i].code == sst_object_codes[0x20]) {
            CHECK_EQ(sst[i].collision_flags, 0x8B); // (they hurt)
            CHECK_EQ(sst[i].y_vel, -0x200);
        }
}

void RegisterHTZFireballTests(void) {
    RUN_TEST(HTZFireball_SubtypeSetsSpeedAndWait);
    RUN_TEST(HTZFireball_ThrowsTwoBallsTheOtherWay);
}
