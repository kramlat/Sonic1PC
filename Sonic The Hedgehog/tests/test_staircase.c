#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/Staircase.h"
#include "ObjectSst.h"
#include "SstLoad.h"

// Object 5B: in the original, Stair_Move falls into Stair_Solid, so the FIRST
// (parent) block is solid and descends with the others. The port skipped
// Stair_Solid for the parent: it never moved, wasn't solid, and Sonic standing
// on it couldn't trigger the stairs.

static Sst *SpawnStairs(uint8_t subtype, int *parent_slot) {
    Sst_Reset();
    for (int i = SST_FIRST_DYNAMIC; i < SST_COUNT; i++) {
        memset(&sst[i], 0xAB, sizeof(Sst));
        sst[i].code = NULL;
    }
    memset(player, 0, sizeof(Sst));
    player->routine = 2;
    player->x_pos.f.u = 0x1000; // far away
    player->y_pos.f.u = 0x1000;
    scrpos_x.f.u = 0x100;
    SstLoadCamera camera = {0x100, 0, false, 0xFFF};
    static const SstObjectIndex index_ = {sst_object_codes, 0x100};
    SstLoad_Init(&index_, NULL, false, &camera);
    *parent_slot = SST_FIRST_LEVEL + 4;
    Sst *p = &sst[*parent_slot];
    memset(p, 0, sizeof(Sst));
    p->code = sst_object_codes[ObjId_Staircase];
    p->x_pos.f.u = 0x180;
    p->y_pos.f.u = 0x200;
    p->subtype = subtype;
    return p;
}

static void Staircase_ParentDescendsWithChildren(void) {
    int slot;
    Sst *p = SpawnStairs(1, &slot); // subtype 1 = already moving down
    Own_Staircase *ps = (Own_Staircase *)p->own.u8;

    for (int f = 0; f < 8; f++)
        Sst_Process(false);
    CHECK_EQ(ps->children_y[0], 8);
    CHECK_EQ(ps->children_y[1], 6); // floor(3*8/4)
    CHECK_EQ(ps->children_y[2], 4);
    CHECK_EQ(ps->children_y[3], 2);
    CHECK_EQ(p->y_pos.f.u, 0x200 + 8); // the parent moved too
}

static void Staircase_ParentIsSolidAndTriggersStairs(void) {
    int slot;
    Sst *p = SpawnStairs(0, &slot);
    Own_Staircase *ps = (Own_Staircase *)p->own.u8;
    Sst_Process(false); // spawn the 4 blocks

    // Stand Sonic on the first block.
    player->x_pos.f.u = p->x_pos.f.u;
    player->y_pos.f.u = p->y_pos.f.u - 16 - 20;
    player->y_radius = 19;
    player->x_radius = 9;
    player->y_vel = 0;
    for (int f = 0; f < 3; f++)
        Sst_Process(false);
    CHECK((p->status & SST_STATUS_PLAYER_STAND) || ps->touch == 1);
}

void RegisterStaircaseTests(void) {
    RUN_TEST(Staircase_ParentDescendsWithChildren);
    RUN_TEST(Staircase_ParentIsSolidAndTriggersStairs);
}
