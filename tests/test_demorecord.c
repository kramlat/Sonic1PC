#include "test.h"

#include <stdio.h>
#include <string.h>

#include "Demo.h"
#include "Game.h"
#include "Level.h"

// In-app demo recording / playback requests (Tools menu): the request is applied on the next frame,
// recording runs until stopped (Start does NOT stop it) and the game keeps running afterwards.

static void Demo_RecordRequestRestartsLevelAndRecordsUntilStopped(void) {
    gamemode = GameMode_Level;
    restart = false;
    last_lamp = 5;

    DemoRecordRequest r = {0};
    r.zone = ZoneId_LZ;
    r.act = 1;
    r.start_x = -1;
    r.start_y = -1;
    r.frames = -1;
    snprintf(r.path, sizeof(r.path), "/tmp/sonic_test_demo.bin");
    remove(r.path);

    Demo_RequestRecording(&r);
    CHECK(Demo_RecordingActive()); // pending counts as active
    CHECK_EQ(level_id, level_id);  // not applied yet

    Demo_ServiceRequests();
    CHECK_EQ(level_id, LEVEL_ID(ZoneId_LZ, 1));
    CHECK(restart);                // a running level restarts itself
    CHECK_EQ(last_lamp, 0);
    CHECK(Demo_RecordingActive());

    // Right for 3 frames, then right+A for 2 frames; pressing Start must not stop the recording.
    for (int i = 0; i < 3; i++) { jpad1_hold1 = JPAD_RIGHT; jpad1_press1 = (i == 1) ? JPAD_START : 0; RecordDemoFrame(); }
    for (int i = 0; i < 2; i++) { jpad1_hold1 = JPAD_RIGHT | JPAD_A; jpad1_press1 = 0; RecordDemoFrame(); }
    CHECK(Demo_RecordingActive());
    CHECK_EQ(Demo_RecordedFrames(), 5);

    Demo_StopRecording();
    CHECK(!Demo_RecordingActive());
    CHECK_EQ(Demo_LastSavedFrames(), 5);

    FILE *f = fopen(r.path, "rb");
    CHECK(f != NULL);
    if (f) {
        uint8_t bytes[16];
        size_t n = fread(bytes, 1, sizeof(bytes), f);
        fclose(f);
        CHECK_EQ(n, 8); // three records ([buttons, duration - 1]), the last being the in-progress one
        CHECK_EQ(bytes[0], 0);                     // the recorder's usual first record: no buttons, 1 frame
        CHECK_EQ(bytes[1], 0);
        CHECK_EQ(bytes[2], JPAD_RIGHT);            // 3 frames of right -> duration 2
        CHECK_EQ(bytes[3], 2);
        CHECK_EQ(bytes[4], (JPAD_RIGHT | JPAD_A)); // 2 frames of right+A -> duration 1
        CHECK_EQ(bytes[5], 1);
    }
}

static void Demo_RecordFromTitleLeavesTheTitle(void) {
    gamemode = GameMode_Title;
    DemoRecordRequest r = {0};
    r.zone = ZoneId_GHZ;
    r.start_x = r.start_y = -1;
    r.frames = 2;
    snprintf(r.path, sizeof(r.path), "/tmp/sonic_test_demo2.bin");
    Demo_RequestRecording(&r);
    Demo_ServiceRequests();
    CHECK_EQ(gamemode, GameMode_Level); // the title loop sees the change and returns to the main loop
    jpad1_hold1 = 0; jpad1_press1 = 0;
    RecordDemoFrame();
    RecordDemoFrame(); // frame limit reached: saved and stopped, no exit
    CHECK(!Demo_RecordingActive());
}

static void Demo_PlaybackRequestLoadsTheDemo(void) {
    // Two records: RIGHT for 10 frames, RIGHT|A for 5 (the second is the in-progress last record).
    uint8_t data[6] = {JPAD_RIGHT, 9, JPAD_RIGHT | JPAD_A, 4, 0, 0};
    gamemode = GameMode_Title;
    DemoPlayRequest p = {ZoneId_MZ, 0, -1, -1};
    CHECK(Demo_RequestPlayback(&p, data, sizeof(data)));
    Demo_ServiceRequests();
    CHECK_EQ(gamemode, GameMode_Demo);
    CHECK_EQ(level_id, LEVEL_ID(ZoneId_MZ, 0));
    CHECK(cli_demo_override != NULL);
    CHECK_EQ(cli_demo_length, 10 + 5 + 30);
    CHECK_EQ(cli_demo_override[0], JPAD_RIGHT);
    cli_demo_override = NULL;
    cli_demo_length = -1;
    demo = 0;
}

// A request made while the game is on a screen that can't be left yet waits for one that can,
// instead of being applied (and lost) right away.
static void Demo_RequestWaitsOnScreensThatCannotBeLeft(void) {
    DemoRecordRequest r = {0};
    r.zone = ZoneId_MZ;
    r.start_x = r.start_y = -1;
    r.frames = -1;
    snprintf(r.path, sizeof(r.path), "/tmp/sonic_test_demo3.bin");

    gamemode = GameMode_Credits;
    Demo_RequestRecording(&r);
    Demo_ServiceRequests();
    CHECK_EQ(gamemode, GameMode_Credits); // untouched
    CHECK(Demo_RecordingActive());        // still pending

    gamemode = GameMode_Sega;             // the Sega screen's loops now leave on a mode change
    Demo_ServiceRequests();
    CHECK_EQ(gamemode, GameMode_Level);
    CHECK_EQ(level_id, LEVEL_ID(ZoneId_MZ, 0));
    Demo_StopRecording();
}

void RegisterDemoRecordTests(void) {
    RUN_TEST(Demo_RequestWaitsOnScreensThatCannotBeLeft);
    RUN_TEST(Demo_RecordRequestRestartsLevelAndRecordsUntilStopped);
    RUN_TEST(Demo_RecordFromTitleLeavesTheTitle);
    RUN_TEST(Demo_PlaybackRequestLoadsTheDemo);
}
