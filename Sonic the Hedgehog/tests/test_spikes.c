#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/Sonic.h"
#include "Object/Spikes.h"
#include "ObjectSst.h"
#include "SstLoad.h"

// Object 36, the spikes: the high nibble of the subtype is their size and direction (a table of frame and width), and they hurt Sonic who lands on them or touches them from the side.

static Sst *Spawn(uint8_t subtype) {
    memset(player, 0, sizeof(Sst));
    player->routine = 2;
    player->y_radius = 19;
    player->x_pos.f.u = 0x1000;
    player->y_pos.f.u = 0x1000;
    scrpos_x.f.u = 0xF00;
    scrpos_y.f.u = 0xF00;
    invincibility = 0;
    Sst_Reset();
    static const SstObjectIndex index_ = {sst_object_codes, 0x100};
    SstLoadCamera camera = {0xF00, 0xF00, false, 0xFFF};
    SstLoad_Init(&index_, NULL, false, &camera);
    Sst *obj = Sst_Spawn(ObjId_Spikes, 0x1000, 0x1040);
    obj->subtype = subtype;
    Obj_Spikes(obj);
    return obj;
}

static void Spikes_SizeAndDirectionComeFromTheHighNibble(void) {
    static const struct { uint8_t subtype, frame, width; } sets[] = {
        { 0x00, 0, 20 }, { 0x10, 1, 10 }, { 0x20, 2, 4 }, { 0x30, 3, 28 }, { 0x40, 4, 64 }, { 0x50, 5, 16 },
    };
    for (size_t i = 0; i < sizeof(sets) / sizeof(sets[0]); i++) {
        Sst *obj = Spawn(sets[i].subtype);
        CHECK_EQ(obj->mapping_frame, sets[i].frame);
        CHECK_EQ(obj->width_pixels, sets[i].width);
    }
}

static void Spikes_HurtWhoLandsOnThem(void) {
    Sst *obj = Spawn(0x00); // up, $28 wide
    player->x_pos.f.u = 0x1000;
    player->y_pos.f.u = 0x1040 - 16 - 19 - 2;
    player->y_vel = 0x400;
    player->status |= SST_PSTATUS_IN_AIR;
    for (int i = 0; i < 6 && player->routine < 4; i++) {
        Obj_Spikes(obj);
        player->y_pos.f.u += 2; // (he is falling on them)
    }
    CHECK(player->routine >= 4); // hurt
}

static void Spikes_DoNotHurtWhoIsInvincible(void) {
    Sst *obj = Spawn(0x00);
    invincibility = 1;
    player->x_pos.f.u = 0x1000;
    player->y_pos.f.u = 0x1040 - 16 - 19 - 2;
    player->y_vel = 0x400;
    player->status |= SST_PSTATUS_IN_AIR;
    for (int i = 0; i < 6; i++) {
        Obj_Spikes(obj);
        player->y_pos.f.u += 2;
    }
    CHECK(player->routine < 4);
}

static void Spikes_ThatMoveDoMove(void) {
    Sst *obj = Spawn(0x01); // moves up and down
    Own_Spikes *scratch = (Own_Spikes *)obj->own.u8;
    const int16_t y0 = scratch->orig_y;
    int moved = 0;
    for (int i = 0; i < 400; i++) {
        Obj_Spikes(obj);
        if (obj->y_pos.f.u != y0)
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
