#include "test.h"

#include <string.h>

#include "HTZQuake.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Sprites.h"

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

static void HTZQuake_TheSecondViewShakesWithTheGround(void) {
    Start(0);
    HTZQuake_SetView(1);
    Camera(0x1000, 0x500); // (the second camera: ShakeView runs with it swapped in)
    memset(hscroll_buffer, 0, sizeof(hscroll_buffer));
    vid_bg_scrpos_y_dup = 0x500;
    frame_count = 1; // (the shake table's entry 1: 2 down, 1 across; entry 0 is 1 and 2)
    htz_shaking = 0;
    HTZQuake_ShakeView();
    CHECK_EQ(htz_p2_shake_y, 0); // (still ground: no shake)
    CHECK_EQ(vid_bg_scrpos_y_dup, 0x500);
    CHECK_EQ(hscroll_buffer[5][0], 0);
    htz_shaking = 1;
    HTZQuake_ShakeView();
    CHECK(htz_p2_shake_y >= 1 && htz_p2_shake_y <= 3);
    CHECK_EQ(vid_bg_scrpos_y_dup, 0x500 + htz_p2_shake_y);
    CHECK(hscroll_buffer[5][0] <= -1 && hscroll_buffer[5][0] >= -3);
    CHECK_EQ(hscroll_buffer[5][0], hscroll_buffer[5][1]);
    CHECK(*sprite_view_p2.layer[1].y == 0x500 + htz_p2_shake_y);
    htz_shaking = 0;
    HTZQuake_SetView(0);
    HTZQuake_Reset();
    CHECK(sprite_view_p2.layer[1].y == &scrpos_y_p2.f.u);
}

static void HTZQuake_TheSecondCameraCanStartTheQuake(void) {
    Start(0);
    Camera(0x1900, 0x500); // (the second camera, swapped in) is in the stretch
    HTZQuake_EventsP2();
    CHECK_EQ(htz_quake, 1);
    CHECK_EQ(HTZQuake_Owner(), 1);
    CHECK_EQ(dle_routine, 2);
    Camera(0x100, 0x500); // the first camera, far from it: its events leave the second's quake be
    HTZQuake_Events();
    CHECK_EQ(htz_quake, 1);
    CHECK_EQ(dle_routine, 2);
}

static void HTZQuake_TheFirstCameraKeepsItsQuakeFromTheSecond(void) {
    Start(0);
    Camera(0x1900, 0x500);
    HTZQuake_Events();
    CHECK_EQ(htz_quake, 1);
    CHECK_EQ(HTZQuake_Owner(), 0);
    Camera(0x100, 0x500); // the second camera elsewhere
    HTZQuake_EventsP2();
    CHECK_EQ(htz_quake, 1);
    CHECK_EQ(dle_routine, 2);
}

void RegisterHTZQuakeTests(void) {
    RUN_TEST(HTZQuake_TheSecondCameraCanStartTheQuake);
    RUN_TEST(HTZQuake_TheFirstCameraKeepsItsQuakeFromTheSecond);
    RUN_TEST(HTZQuake_TheSecondViewShakesWithTheGround);
    RUN_TEST(HTZQuake_ActOneIsQuietOutsideItsStretch);
    RUN_TEST(HTZQuake_ActOneStartsAtTheStretchesEntrance);
    RUN_TEST(HTZQuake_TheGroundRestsThenTurnsBack);
    RUN_TEST(HTZQuake_LeavingTheStretchResetsTheBackground);
    RUN_TEST(HTZQuake_ActTwoStartsLowOrHighByTheCamera);
}
