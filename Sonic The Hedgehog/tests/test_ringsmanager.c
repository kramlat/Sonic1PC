#include "test.h"

#include <string.h>

#include "RingsManager.h"

// The engine's rings manager (ParadoxEngine/src/RingsManager.c) on a made-up ring layout.

#define X(v) (uint8_t)((v) >> 8), (uint8_t)(v)
#define RING(x, y, count, vertical) X(x), X((y) | (((count) - 1) << 12) | ((vertical) ? 0x8000 : 0))

static const uint8_t layout[] = {
    RING(0x100, 0x40, 1, 0),  // entry 0: one ring
    RING(0x200, 0x40, 3, 0),  // entry 1: three rings in a row, 0x200 0x218 0x230
    RING(0x300, 0x40, 2, 1),  // entry 2: two rings in a column
    RING(0x800, 0x40, 1, 0),  // entry 3: far away
    X(0xFFFF), X(0),
};

typedef struct { int16_t x, y; uint16_t entry; uint8_t bit; } Spawned;
static Spawned spawned[64];
static int spawn_count;

static void Spawn(void *user, const RingSpawn *r) {
    (void)user;
    spawned[spawn_count++] = (Spawned){ r->x, r->y, r->entry, r->bit };
}

static RingsManager mgr;
static uint8_t status[16];

static void Reset(int16_t camera_x) {
    static const ObjectsManagerConfig config = OBJECTS_MANAGER_DEFAULT_CONFIG;
    spawn_count = 0;
    RingsManager_Init(&mgr, &config, status, sizeof(status), layout, camera_x, Spawn, NULL);
}

static void RingsManager_LoadsTheStartWindow(void) {
    Reset(0);
    // Everything below 0x280: the single ring and the row of three
    CHECK_EQ(spawn_count, 4);
    CHECK_EQ(spawned[0].x, 0x100);
    CHECK_EQ(spawned[1].x, 0x200);
    CHECK_EQ(spawned[2].x, 0x218);
    CHECK_EQ(spawned[3].x, 0x230);
    CHECK_EQ(spawned[3].bit, 2);
    CHECK_EQ(spawned[3].entry, 1);
}

static void RingsManager_ColumnsGoDown(void) {
    Reset(0x100);
    bool found = false;
    for (int i = 0; i < spawn_count; i++)
        if (spawned[i].entry == 2) {
            found = true;
            CHECK_EQ(spawned[i].x, 0x300);
            CHECK_EQ(spawned[i].y, 0x40 + spawned[i].bit * RING_SPACING);
        }
    CHECK(found);
}

static void RingsManager_LoadsAheadWhenMovingRight(void) {
    Reset(0);
    spawn_count = 0;
    RingsManager_Update(&mgr, 0x100, Spawn, NULL); // window to 0x380: entry 2 (two rings)
    CHECK_EQ(spawn_count, 2);
    RingsManager_Update(&mgr, 0x110, Spawn, NULL); // the same 128-pixel column: nothing
    CHECK_EQ(spawn_count, 2);
    RingsManager_Update(&mgr, 0x600, Spawn, NULL); // 0x880: the far one
    CHECK_EQ(spawn_count, 3);
    CHECK_EQ(spawned[2].x, 0x800);
}

// The camera walks back a column at a time, as it does in the game (a jump is not something it does)
static void WalkBack(int16_t from, int16_t to) {
    for (int16_t x = from; x >= to; x -= 0x80)
        RingsManager_Update(&mgr, x, Spawn, NULL);
}

static void RingsManager_CollectedRingsDoNotComeBack(void) {
    Reset(0);
    RingsManager_Collect(&mgr, 1, 1); // the middle ring of the row
    CHECK(RingsManager_Collected(&mgr, 1, 1));
    CHECK(!RingsManager_Collected(&mgr, 1, 0));
    RingsManager_Update(&mgr, 0x600, Spawn, NULL); // the camera goes away ...
    spawn_count = 0;
    WalkBack(0x580, 0x80);                         // ... and comes back: the row loads again, without the collected one
    int row = 0;
    for (int i = 0; i < spawn_count; i++)
        if (spawned[i].entry == 1) {
            row++;
            CHECK(spawned[i].bit != 1);
        }
    CHECK_EQ(row, 2);
}

static void RingsManager_ReloadsWhatLeftAndReturns(void) {
    Reset(0);
    RingsManager_Update(&mgr, 0x600, Spawn, NULL);
    spawn_count = 0;
    WalkBack(0x580, 0x80);
    CHECK(spawn_count >= 4); // the single ring and the row are made again
}

static void RingsManager_NoLayoutIsNoRings(void) {
    static const ObjectsManagerConfig config = OBJECTS_MANAGER_DEFAULT_CONFIG;
    spawn_count = 0;
    RingsManager_Init(&mgr, &config, status, sizeof(status), NULL, 0, Spawn, NULL);
    RingsManager_Update(&mgr, 0x400, Spawn, NULL);
    CHECK_EQ(spawn_count, 0);
}

static void RingsManager_EntriesPastTheTableAreNotRemembered(void) {
    static const ObjectsManagerConfig config = OBJECTS_MANAGER_DEFAULT_CONFIG;
    spawn_count = 0;
    RingsManager_Init(&mgr, &config, status, 2, layout, 0, Spawn, NULL); // a table of two entries
    RingsManager_Collect(&mgr, 3, 0);
    CHECK(!RingsManager_Collected(&mgr, 3, 0));
}

void RegisterRingsManagerTests(void) {
    RUN_TEST(RingsManager_LoadsTheStartWindow);
    RUN_TEST(RingsManager_ColumnsGoDown);
    RUN_TEST(RingsManager_LoadsAheadWhenMovingRight);
    RUN_TEST(RingsManager_CollectedRingsDoNotComeBack);
    RUN_TEST(RingsManager_ReloadsWhatLeftAndReturns);
    RUN_TEST(RingsManager_NoLayoutIsNoRings);
    RUN_TEST(RingsManager_EntriesPastTheTableAreNotRemembered);
}
