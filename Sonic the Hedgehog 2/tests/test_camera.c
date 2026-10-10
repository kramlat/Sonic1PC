#include "test.h"

#include <string.h>

#include "Camera.h"
#include "Constants.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/Tails.h"
#include "SplitScreen.h"

// The camera: how it follows Sonic (the dead zone, the 16 pixel cap, the limits), the end sign's lock, and the split screen's second camera, which follows Tails with limits of its own. The sign locks the
// cameras of the views it is in sight of and no other (the first version set the one shared limit, which dragged the second view to the end of the level too).

extern uint8_t bg1_xblock, bg1_yblock;
extern void ScrollHoriz(void);
extern void ScrollVertical(void);

enum { Follow = 144 };

static void Setup(bool split) {
    memset(objects, 0, sizeof(Object) * 0x40);
    level_id = LEVEL_ID(ZoneId_EHZ, 0);
    limit_left1 = limit_left2 = 0;
    limit_right1 = limit_right2 = 0x2000;
    limit_top1 = limit_top2 = 0;
    limit_btm1 = limit_btm2 = 0x400;
    cam_x_delay = 0;
    look_shift = 96 + SCREEN_TALLADD2;
    bgscrollvert = false;
    scrpos_x.v = scrpos_y.v = 0;
    scrpos_x.f.u = 0x400;
    scrpos_y.f.u = 0x100;
    player->routine = 2;
    player->status.b = 0;
    player->inertia = 0;
    player->pos.l.x.f.u = 0x400 + Follow;
    player->pos.l.y.f.u = 0x100 + look_shift;
    two_player_mode = split;
    SplitScreen_LoadLevel();
}

static void Teardown(void) {
    two_player_mode = 0;
    SplitScreen_LoadLevel();
}

// --- following Sonic ---

static void Camera_StandingAtTheFollowingPlaceHoldsStill(void) {
    Setup(false);
    player->pos.l.x.f.u = 0x400 + Follow + 10; // (a dead zone of 16 pixels to the right of the place)
    ScrollHoriz();
    CHECK_EQ(scrpos_x.f.u, 0x400);
    CHECK_EQ(scrshift_x, 0);
}

static void Camera_RunsAheadByTheExcess(void) {
    Setup(false);
    player->pos.l.x.f.u = 0x400 + Follow + 16 + 5;
    ScrollHoriz();
    CHECK_EQ(scrpos_x.f.u, 0x405);
}

static void Camera_RunsAheadBySixteenAtMost(void) {
    Setup(false);
    player->pos.l.x.f.u = 0x400 + Follow + 200;
    ScrollHoriz();
    CHECK_EQ(scrpos_x.f.u, 0x410);
}

static void Camera_FallsBackWhenSonicIsBehind(void) {
    Setup(false);
    player->pos.l.x.f.u = 0x400 + Follow - 6;
    ScrollHoriz();
    CHECK_EQ(scrpos_x.f.u, 0x400 - 6);
}

static void Camera_StopsAtTheLeftAndRightLimits(void) {
    Setup(false);
    limit_left2 = 0x3F0;
    player->pos.l.x.f.u = 0x400 + Follow - 20; // (behind the following place: the camera would fall back 20)
    ScrollHoriz();
    CHECK_EQ(scrpos_x.f.u, 0x3F0);

    Setup(false);
    limit_right2 = 0x408;
    player->pos.l.x.f.u = 0x400 + Follow + 100;
    ScrollHoriz();
    CHECK_EQ(scrpos_x.f.u, 0x408);
}

static void Camera_StandingStillOnTheGroundIsNotScrolledVertically(void) {
    Setup(false);
    ScrollVertical();
    CHECK_EQ(scrpos_y.f.u, 0x100);
}

static void Camera_SnapsWhenCloseOnTheGround(void) {
    Setup(false);
    player->pos.l.y.f.u += 4; // (inside the 6 pixels that a walking pace takes at once)
    ScrollVertical();
    CHECK_EQ(scrpos_y.f.u, 0x104);
}

static void Camera_VerticalIsCappedAtTheBottomLimit(void) {
    Setup(false);
    limit_btm2 = 0x102;
    player->pos.l.y.f.u += 100;
    ScrollVertical();
    CHECK_EQ(scrpos_y.f.u, 0x102);
}

static void Camera_VerticalIsCappedAtTheTopLimit(void) {
    Setup(false);
    limit_top2 = 0x0FE;
    player->pos.l.y.f.u -= 100;
    ScrollVertical();
    CHECK_EQ(scrpos_y.f.u, 0x0FE);
}

// --- the end sign ---

static void Lock_WithoutASplitScreenTheOneCameraIsLocked(void) {
    Setup(false);
    SplitScreen_LockCameras(0x500, 24);
    CHECK_EQ(limit_left2, limit_right2);
}

// Locked, the left limit is the right limit: the camera that would fall back a pixel is held at the end of the level instead
static void Lock_TheLockedCameraRunsToTheEnd(void) {
    Setup(false);
    SplitScreen_LockCameras(0x500, 24);
    player->pos.l.x.f.u = 0x400 + Follow - 1;
    ScrollHoriz();
    CHECK_EQ(scrpos_x.f.u, limit_right2);
}

// --- the split screen ---

static int16_t ViewWidth(void) {
    return (int16_t)(SplitScreen_FollowX() * 2 + 32);
}

static void Split_TwoPlayerModeMakesTwoViews(void) {
    Setup(true);
    CHECK(SplitScreen_Active());
    CHECK_EQ(scrpos_x_p2.f.u, scrpos_x.f.u); // (both start where Sonic is)
    CHECK_EQ(scrpos_y_p2.f.u, scrpos_y.f.u);
    Teardown();
    CHECK(!SplitScreen_Active());
}

static void Split_OnePlayerModeHasNoSecondView(void) {
    Setup(false);
    CHECK(!SplitScreen_Active());
}

// The sign, in sight of the first view only: only the first camera is locked
static void Split_SignInTheFirstViewLocksTheFirstOnly(void) {
    Setup(true);
    scrpos_x_p2.f.u = 0x1000; // (the second camera is far away)
    SplitScreen_LockCameras(0x400 + 20, 24);
    CHECK_EQ(limit_left2, limit_right2);

    // ... and the second camera is not held up by it: it follows Tails, to the left of where the first is locked
    TAILS_OBJ->pos.l.x.f.u = 0x1000 + SplitScreen_FollowX();
    TAILS_OBJ->pos.l.y.f.u = scrpos_y_p2.f.u + 96;
    SplitScreen_Scroll();
    CHECK_EQ(scrpos_x_p2.f.u, 0x1000);
    Teardown();
}

static void Split_SignInTheSecondViewLocksTheSecondOnly(void) {
    Setup(true);
    scrpos_x_p2.f.u = 0x1000;
    SplitScreen_LockCameras(0x1000 + 20, 24);
    CHECK_EQ(limit_left2, 0); // (the first camera is free)

    // The second camera runs to the right limit and stays: Tails behind the following place does not draw it back
    TAILS_OBJ->pos.l.x.f.u = 0x1000 + SplitScreen_FollowX();
    TAILS_OBJ->pos.l.y.f.u = scrpos_y_p2.f.u + 96;
    SplitScreen_Scroll();
    CHECK_EQ(scrpos_x_p2.f.u, limit_right2);
    Teardown();
}

static void Split_TheEndOfTheLevelHoldsOnlyTheViewThatReachedIt(void) {
    Setup(true);
    const int16_t end_x = 0x1000;
    scrpos_x.f.u = 0x1010; // the first camera has got there, the second has not
    scrpos_x_p2.f.u = 0x400;
    limit_left2 = 0;
    CHECK(SplitScreen_ReachEnd(end_x)); // (the sign's art loads)
    CHECK_EQ(limit_left2, end_x);
    TAILS_OBJ->pos.l.x.f.u = 0x400 + SplitScreen_FollowX() - 8;
    TAILS_OBJ->pos.l.y.f.u = scrpos_y_p2.f.u + 96;
    SplitScreen_Scroll();
    CHECK_EQ(scrpos_x_p2.f.u, 0x400 - 8); // (the second view is still free to go back)
    scrpos_x_p2.f.u = 0x1020; // now the second gets there: held, and no second load of the art
    CHECK(!SplitScreen_ReachEnd(end_x));
    TAILS_OBJ->pos.l.x.f.u = 0x800; // (Tails far behind pulls the camera back, but not past the end's limit)
    for (int i = 0; i < 4; i++)
        SplitScreen_Scroll();
    CHECK_EQ(scrpos_x_p2.f.u, end_x);
    Teardown();
}

static void Split_SignInSightOfNeitherViewLocksNeither(void) {
    Setup(true);
    scrpos_x_p2.f.u = 0x1000;
    SplitScreen_LockCameras(0x1800, 24);
    CHECK_EQ(limit_left2, 0);
    TAILS_OBJ->pos.l.x.f.u = 0x1000 + SplitScreen_FollowX() - 8;
    TAILS_OBJ->pos.l.y.f.u = scrpos_y_p2.f.u + 96;
    SplitScreen_Scroll();
    CHECK_EQ(scrpos_x_p2.f.u, 0x1000 - 8); // (free: it fell back after Tails)
    Teardown();
}

static void Split_SignInSightOfBothLocksBoth(void) {
    Setup(true);
    scrpos_x_p2.f.u = 0x400 + 8; // (the views overlap)
    SplitScreen_LockCameras(0x400 + 20, 24);
    CHECK_EQ(limit_left2, limit_right2);
    TAILS_OBJ->pos.l.x.f.u = 0x400 + 8 + SplitScreen_FollowX();
    TAILS_OBJ->pos.l.y.f.u = scrpos_y_p2.f.u + 96;
    SplitScreen_Scroll();
    CHECK_EQ(scrpos_x_p2.f.u, limit_right2);
    Teardown();
}

// The edge of the view: in sight is within the view's width (and the object's own width each way)
static void Split_SignJustOutsideTheViewIsNotInSight(void) {
    Setup(true);
    scrpos_x_p2.f.u = 0x2000 - 0x400; // (clear of the first)
    int16_t w = ViewWidth();
    SplitScreen_LockCameras((int16_t)(0x400 + w + 23), 24); // (its left edge a pixel inside the view's right edge: in)
    CHECK_EQ(limit_left2, limit_right2);
    limit_left2 = 0;
    SplitScreen_LockCameras((int16_t)(0x400 + w + 24), 24); // (at the edge: out)
    CHECK_EQ(limit_left2, 0);
    Teardown();
}

// The second view's background has block trackers of its own: running its deformation must leave the first's alone
static void Split_SecondViewsBackgroundLeavesTheFirstsTrackersAlone(void) {
    Setup(true);
    level_id = LEVEL_ID(ZoneId_HPZ, 0); // (Hidden Palace's background moves by the block-tracked scroll)
    bg1_xblock = 0x10;
    bg1_yblock = 0x10;
    for (int i = 0; i < 60; i++) { // Tails runs ahead for 60 frames: the second camera, and so its background, moves some hundreds of pixels
        TAILS_OBJ->pos.l.x.f.u = scrpos_x_p2.f.u + SplitScreen_FollowX() + 16 + 40;
        TAILS_OBJ->pos.l.y.f.u = scrpos_y_p2.f.u + 96;
        SplitScreen_Scroll();
    }
    CHECK(scrpos_x_p2.f.u > 0x400 + 300);
    CHECK_EQ(bg1_xblock, 0x10);
    CHECK_EQ(bg1_yblock, 0x10);
    Teardown();
}

void RegisterCameraTests(void) {
    RUN_TEST(Camera_StandingAtTheFollowingPlaceHoldsStill);
    RUN_TEST(Camera_RunsAheadByTheExcess);
    RUN_TEST(Camera_RunsAheadBySixteenAtMost);
    RUN_TEST(Camera_FallsBackWhenSonicIsBehind);
    RUN_TEST(Camera_StopsAtTheLeftAndRightLimits);
    RUN_TEST(Camera_StandingStillOnTheGroundIsNotScrolledVertically);
    RUN_TEST(Camera_SnapsWhenCloseOnTheGround);
    RUN_TEST(Camera_VerticalIsCappedAtTheBottomLimit);
    RUN_TEST(Camera_VerticalIsCappedAtTheTopLimit);
    RUN_TEST(Lock_WithoutASplitScreenTheOneCameraIsLocked);
    RUN_TEST(Lock_TheLockedCameraRunsToTheEnd);
    RUN_TEST(Split_TwoPlayerModeMakesTwoViews);
    RUN_TEST(Split_OnePlayerModeHasNoSecondView);
    RUN_TEST(Split_SignInTheFirstViewLocksTheFirstOnly);
    RUN_TEST(Split_SignInTheSecondViewLocksTheSecondOnly);
    RUN_TEST(Split_TheEndOfTheLevelHoldsOnlyTheViewThatReachedIt);
    RUN_TEST(Split_SignInSightOfNeitherViewLocksNeither);
    RUN_TEST(Split_SignInSightOfBothLocksBoth);
    RUN_TEST(Split_SignJustOutsideTheViewIsNotInSight);
    RUN_TEST(Split_SecondViewsBackgroundLeavesTheFirstsTrackersAlone);
}
