#include "test.h"

#include <string.h>

#include "HTZQuake.h"
#include "Level.h"
#include "LevelScroll.h"

// Hill Top's earthquake (the prototype's DynResize_HTz, loc_7AE4): the events switch a quake's stretch of the level on and off by the camera's place, and move the ground between two offsets.

static void Camera(int x, int y) {
    scrpos_x.v = (int32_t)x << 16;
    scrpos_y.v = (int32_t)y << 16;
}

static void Start(int act) {
    level_id = (uint16_t)LEVEL_ID(ZoneId_HTZ, act);
    dle_routine = 0;
    HTZQuake_Reset();
    bg_scrpos_x.v = 0;
    bg_scrpos_y.v = 0;
    scrshift_x = scrshift_y = 0;
    frame_count = 0;
}

static void HTZQuake_ActOneIsQuietOutsideItsStretch(void) {
    Start(0);
    Camera(0x1000, 0x500);
    HTZQuake_Events();
    CHECK_EQ(htz_quake, 0);
    CHECK_EQ(dle_routine, 0);
    Camera(0x1900, 0x300); // right place, but not low enough
    HTZQuake_Events();
    CHECK_EQ(htz_quake, 0);
}

static void HTZQuake_ActOneStartsAtTheStretchesEntrance(void) {
    Start(0);
    Camera(0x1800, 0x400);
    HTZQuake_Events();
    CHECK_EQ(htz_quake, 1);
    CHECK_EQ(dle_routine, 2);
    CHECK_EQ(htz_bg_y_offset, 0x140);
    CHECK_EQ(bg_scrpos_x.f.u, 0x1800);
    CHECK_EQ(bg_scrpos_y.f.u, 0x400 - 0x100); // (the background starts $100 above the camera)
}

static void Tick(void) {
    frame_count++;
    HTZQuake_Events();
}

static void HTZQuake_TheGroundRestsThenTurnsBack(void) {
    Start(0);
    Camera(0x1800, 0x400);
    HTZQuake_Events();
    htz_bg_y_offset = 0x140;
    // The delay starts at 0: arriving at the top for the first time the ground turns at once, with the screen shaking
    Tick();
    CHECK_EQ(htz_shaking, 1);
    // ... and goes down, a pixel every fourth frame, to $E0
    int frames = 0;
    while (htz_bg_y_offset > 0xE0 && frames < 2000) {
        Tick();
        frames++;
    }
    CHECK_EQ(htz_bg_y_offset, 0xE0);
    CHECK(frames >= (0x140 - 0xE0) * 4 - 4 && frames <= (0x140 - 0xE0) * 4 + 4);
    // At the bottom it rests $79 calls (the delay of $78 counts down through zero) with the screen still, then turns and the shaking starts again
    int calls = 0;
    do {
        Tick();
        calls++;
        if (calls == 1)
            CHECK_EQ(htz_shaking, 0);
    } while (htz_shaking == 0 && calls < 0x100);
    CHECK_EQ(calls, 0x79);
    CHECK_EQ(htz_shaking, 1);
}

static void HTZQuake_LeavingTheStretchResetsTheBackground(void) {
    Start(0);
    Camera(0x1800, 0x400);
    HTZQuake_Events();
    htz_bg_y_offset = 0x140;
    Camera(0x1F00, 0x400); // past the stretch's end
    HTZQuake_Events();
    CHECK_EQ(dle_routine, 4);
    CHECK_EQ(bg_scrpos_x.v, 0x04000000);
    CHECK_EQ(htz_bg_y_offset, 0);
}

static void HTZQuake_ActTwoStartsLowOrHighByTheCamera(void) {
    Start(1);
    Camera(0x14C0, 0x200);
    HTZQuake_Events();
    CHECK_EQ(htz_quake, 1);
    CHECK_EQ(dle_routine, 2);
    CHECK_EQ(htz_bg_y_offset, 0x2C0);

    Start(1);
    Camera(0x14C0, 0x380); // low enough to start in the second part of the stretch
    HTZQuake_Events();
    CHECK_EQ(dle_routine, 8);
    CHECK_EQ(htz_bg_y_offset, 0x300);
    CHECK_EQ(bg_scrpos_x.f.u, 0x14C0 + 0x480);
}

void RegisterHTZQuakeTests(void) {
    RUN_TEST(HTZQuake_ActOneIsQuietOutsideItsStretch);
    RUN_TEST(HTZQuake_ActOneStartsAtTheStretchesEntrance);
    RUN_TEST(HTZQuake_TheGroundRestsThenTurnsBack);
    RUN_TEST(HTZQuake_LeavingTheStretchResetsTheBackground);
    RUN_TEST(HTZQuake_ActTwoStartsLowOrHighByTheCamera);
}
