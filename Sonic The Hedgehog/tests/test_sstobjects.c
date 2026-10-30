#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "SstLoad.h"

// The Sonic 1 objects that have moved to the status table, loaded from a layout by the loader of that table and run by its handler.

static const SstObjectIndex index_ = {sst_object_codes, 0x100};

#define X(v) (uint8_t)((v) >> 8), (uint8_t)(v)
#define ENTRY(x, y, id, sub) X(x), X(y), (id), (sub)

static const uint8_t layout[] = {
    ENTRY(0x100, 0x100, ObjId_Waterfall, 0), ENTRY(0x120, 0x110, ObjId_Pylon, 0), X(0xFFFF), 0, 0, 0, 0,
};

static SstLoadCamera Cam(int16_t x) {
    SstLoadCamera c = {x, 0, false, 0xFFF};
    return c;
}

static Sst *Of(int entry) {
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code != NULL && sst[i].respawn == entry + 1)
            return &sst[i];
    return NULL;
}

static void Start(void) {
    Sst_Reset();
    scrpos_x.v = scrpos_y.v = 0;
    SstLoadCamera c = Cam(0);
    SstLoad_Init(&index_, layout, false, &c);
}

static void SstObjects_TheLoaderMakesThemFromTheLayout(void) {
    Start();
    CHECK(Of(0) != NULL);
    CHECK(Of(0)->code == sst_object_codes[ObjId_Waterfall]);
    CHECK(Of(1) != NULL);
    CHECK(Of(1)->code == sst_object_codes[ObjId_Pylon]);
    CHECK_EQ(Of(1)->y_pos.f.u, 0x110);
}

static void SstObjects_ThePylonScrollsAgainstTheCameraAtTwiceItsSpeed(void) {
    Start();
    scrpos_x.f.u = 0x20;
    scrpos_y.f.u = 0x10;
    Sst_Process(false);                   // (its first run sets it up, and draws)
    Sst *pylon = Of(1);
    CHECK_EQ(pylon->routine, 2);
    CHECK_EQ(pylon->render_flags, 0);     // (placed on the screen)
    CHECK_EQ(pylon->x_pos.f.u, -0x40);
    CHECK_EQ(pylon->y_pos.f.u, -0x20 + 0x100);
    CHECK_EQ(Sst_QueueCount(0), 1);
    CHECK(Sst_QueueEntry(0, 0) == pylon);
}

static void SstObjects_TheWaterfallCountsSixtyFourFramesAndLeavesWhenTheCameraDoes(void) {
    Start();
    Sst_Process(false);
    Sst_Process(false);                   // (the counter ran out and started again)
    Sst *fall = Of(0);
    CHECK_EQ(fall->own.u8[0], 0x3F);
    Sst_Process(false);
    CHECK_EQ(fall->own.u8[0], 0x3E);
    SstLoadCamera c = Cam(0x2000);
    SstLoad_Update(&c);
    Sst_Process(false);
    CHECK(Of(0) == NULL);                 // deleted ...
    CHECK(!(sst_respawn_table[0] & 0x80)); // ... and its place given back: it comes again when the camera does
}

static void SstObjects_PointsRiseAndSlowAndAreDeletedWhenTheySettle(void) {
    Start();
    Sst *points = Sst_Spawn(ObjId_Points, 0x100, 0x200);
    CHECK(points != NULL);
    points->mapping_frame = 2;
    Sst_Process(false);
    CHECK_EQ(points->y_vel, -0x300 + 0x18);
    CHECK_EQ(points->y_pos.f.u, 0x200 - 3);            // (moved by the speed it had: -0x300 is -3 pixels)
    CHECK_EQ(points->render_flags & SST_RENDER_LEVEL, SST_RENDER_LEVEL);
    int frames = 1;
    while (points->code != NULL && frames < 100) {
        Sst_Process(false);
        frames++;
    }
    CHECK_EQ(frames, 33);                              // (0x300 / 0x18 = 32 slowings, deleted on the next run)
}

static void SstObjects_ABumperThrowsSonicAwayAndPaysTenPointsTenTimes(void) {
    Start();
    memset(objects, 0, sizeof(Object) * 0x20);
    player->type = 1;
    player->pos.l.x.f.u = 0x100;
    player->pos.l.y.f.u = 0x120;
    score = 0;
    Sst *bumper = Sst_Spawn(ObjId_Bumper, 0x100, 0x100);
    sst_respawn_table[0] = 0x80;
    bumper->respawn = 1;
    Sst_Process(false);
    CHECK_EQ(bumper->routine, 2);
    CHECK_EQ(bumper->collision_flags, 0x17 | 0xC0);
    CHECK_EQ(player->ysp, 0);                            // (not touched yet)
    for (int hit = 1; hit <= 12; hit++) {
        bumper->collision_property = 1;                  // (what the touch response does)
        Sst_Process(false);
        CHECK_EQ(player->ysp, 0x700);                    // (the bumper is above him: thrown down)
        CHECK_EQ(bumper->anim, 1);
        CHECK_EQ(score, hit <= 10 ? hit : 10);           // (the tenth hit is the last that pays)
        for (int i = SST_FIRST_DYNAMIC; i < SST_COUNT; i++)
            if (sst[i].code != NULL && sst[i].code != bumper->code)
                Sst_Delete(&sst[i]);                     // (the points that rose)
    }
    CHECK_EQ(sst_respawn_table[0], 0x80 + 10);
}

void RegisterSstObjectsTests(void) {
    RUN_TEST(SstObjects_ABumperThrowsSonicAwayAndPaysTenPointsTenTimes);
    RUN_TEST(SstObjects_PointsRiseAndSlowAndAreDeletedWhenTheySettle);
    RUN_TEST(SstObjects_TheLoaderMakesThemFromTheLayout);
    RUN_TEST(SstObjects_ThePylonScrollsAgainstTheCameraAtTwiceItsSpeed);
    RUN_TEST(SstObjects_TheWaterfallCountsSixtyFourFramesAndLeavesWhenTheCameraDoes);
}
