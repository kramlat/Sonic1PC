#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/Sonic.h"
// (Spikes.h would define the object's art a second time: its scratch layout is repeated here)
typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad1[0x7]; // 0x29-0x2F
    int16_t orig_x;    // 0x30
    int16_t orig_y;    // 0x32
    word_u move;       // 0x34
    uint16_t dir;      // 0x36
    uint16_t timer;    // 0x38
} Scratch_Spikes;

void Obj_Spikes(Object *obj);

// Object 36, the spikes: the high nibble of the subtype is their size and direction (a table of frame and width), and they hurt Sonic who lands on them or touches them from the side.

static Object *Spawn(uint8_t subtype) {
    memset(player, 0, sizeof(Object));
    player->routine = 2;
    player->y_rad = 19;
    player->pos.l.x.f.u = 0x1000;
    player->pos.l.y.f.u = 0x1000;
    scrpos_x.f.u = 0xF00;
    scrpos_y.f.u = 0xF00;
    invincibility = 0;
    Object *obj = &level_objects[4];
    memset(obj, 0, sizeof(Object));
    obj->type = ObjId_Spikes;
    obj->scratch.u8[0] = subtype;
    obj->pos.l.x.f.u = 0x1000;
    obj->pos.l.y.f.u = 0x1040;
    Obj_Spikes(obj);
    return obj;
}

static void Spikes_SizeAndDirectionComeFromTheHighNibble(void) {
    static const struct { uint8_t subtype, frame, width; } sets[] = {
        { 0x00, 0, 20 }, { 0x10, 1, 10 }, { 0x20, 2, 4 }, { 0x30, 3, 28 }, { 0x40, 4, 64 }, { 0x50, 5, 16 },
    };
    for (size_t i = 0; i < sizeof(sets) / sizeof(sets[0]); i++) {
        Object *obj = Spawn(sets[i].subtype);
        CHECK_EQ(obj->frame, sets[i].frame);
        CHECK_EQ(obj->width_pixels, sets[i].width);
    }
}

static void Spikes_HurtWhoLandsOnThem(void) {
    Object *obj = Spawn(0x00); // up, $28 wide
    player->pos.l.x.f.u = 0x1000;
    player->pos.l.y.f.u = 0x1040 - 16 - 19 - 2;
    player->ysp = 0x400;
    player->status.p.f.in_air = true;
    for (int i = 0; i < 6 && player->routine < 4; i++) {
        Obj_Spikes(obj);
        player->pos.l.y.f.u += 2; // (he is falling on them)
    }
    CHECK(player->routine >= 4); // hurt
}

static void Spikes_DoNotHurtWhoIsInvincible(void) {
    Object *obj = Spawn(0x00);
    invincibility = 1;
    player->pos.l.x.f.u = 0x1000;
    player->pos.l.y.f.u = 0x1040 - 16 - 19 - 2;
    player->ysp = 0x400;
    player->status.p.f.in_air = true;
    for (int i = 0; i < 6; i++) {
        Obj_Spikes(obj);
        player->pos.l.y.f.u += 2;
    }
    CHECK(player->routine < 4);
}

static void Spikes_ThatMoveDoMove(void) {
    Object *obj = Spawn(0x01); // moves up and down
    Scratch_Spikes *scratch = (Scratch_Spikes *)&obj->scratch;
    const int16_t y0 = scratch->orig_y;
    int moved = 0;
    for (int i = 0; i < 400; i++) {
        Obj_Spikes(obj);
        if (obj->pos.l.y.f.u != y0)
            moved = 1;
    }
    CHECK(moved);
}

void RegisterSpikesTests(void) {
    RUN_TEST(Spikes_SizeAndDirectionComeFromTheHighNibble);
    RUN_TEST(Spikes_HurtWhoLandsOnThem);
    RUN_TEST(Spikes_DoNotHurtWhoIsInvincible);
    RUN_TEST(Spikes_ThatMoveDoMove);
}
