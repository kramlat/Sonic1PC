#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/Staircase.h"

// Object 5B: in the original, Stair_Move falls into Stair_Solid, so the FIRST
// (parent) block is solid and descends with the others. The port skipped
// Stair_Solid for the parent: it never moved, wasn't solid, and Sonic standing
// on it couldn't trigger the stairs.

static Object *SpawnStairs(uint8_t subtype, int *parent_slot) {
    for (int i = 0; i < LEVEL_OBJECTS; i++) {
        memset(&level_objects[i], 0xAB, sizeof(Object));
        level_objects[i].type = ObjId_Null;
    }
    memset(player, 0, sizeof(Object));
    player->routine = 2;
    player->pos.l.x.f.u = 0x1000; // far away
    player->pos.l.y.f.u = 0x1000;
    scrpos_x.f.u = 0x100;
    *parent_slot = 4;
    Object *p = &level_objects[4];
    memset(p, 0, sizeof(Object));
    p->type = ObjId_Staircase;
    p->pos.l.x.f.u = 0x180;
    p->pos.l.y.f.u = 0x200;
    p->scratch.u8[0] = subtype;
    return p;
}

static void Staircase_ParentDescendsWithChildren(void) {
    int slot;
    Object *p = SpawnStairs(1, &slot); // subtype 1 = already moving down
    Scratch_Staircase *ps = (Scratch_Staircase *)&p->scratch;

    for (int f = 0; f < 8; f++) {
        for (int i = slot; i < slot + 4; i++)
            Obj_Staircase(&objects[i + RESERVED_OBJECTS]);
    }
    CHECK_EQ(ps->children_y[0], 8);
    CHECK_EQ(ps->children_y[1], 6); // floor(3*8/4)
    CHECK_EQ(ps->children_y[2], 4);
    CHECK_EQ(ps->children_y[3], 2);
    CHECK_EQ(p->pos.l.y.f.u, 0x200 + 8); // the parent moved too
}

static void Staircase_ParentIsSolidAndTriggersStairs(void) {
    int slot;
    Object *p = SpawnStairs(0, &slot);
    Scratch_Staircase *ps = (Scratch_Staircase *)&p->scratch;
    Obj_Staircase(p); // spawn the 4 blocks

    // Stand Sonic on the first block.
    player->pos.l.x.f.u = p->pos.l.x.f.u;
    player->pos.l.y.f.u = p->pos.l.y.f.u - 16 - 20;
    player->y_rad = 19;
    player->x_rad = 9;
    player->ysp = 0;
    for (int f = 0; f < 3; f++)
        Obj_Staircase(p);
    CHECK(p->status.o.f.player_stand || ps->touch == 1);
}

void RegisterStaircaseTests(void) {
    RUN_TEST(Staircase_ParentDescendsWithChildren);
    RUN_TEST(Staircase_ParentIsSolidAndTriggersStairs);
}
