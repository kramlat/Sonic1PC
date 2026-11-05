#include "test.h"

#include <string.h>

#include "EngineObject.h"
#include "Rings.h"
#include "ScrollEngine.h"
#include "Sst.h"
#include "Video.h"

#include "Level.h"

// The engine's ring handler (ParadoxEngine/src/Rings.c) on a made-up ring layout.

#define X(v) (uint8_t)((v) >> 8), (uint8_t)(v)

static const uint8_t layout[] = {
    X(0x100), X(0x40),   // 0: a line of three, $18 apart
    X(0x118), X(0x40),   // 1
    X(0x130), X(0x40),   // 2
    X(0x200), X(0x80),   // 3: a column of two
    X(0x200), X(0x98),   // 4
    X(0x800), X(0x40),   // 5: far away
    X(0xFFFF), X(0),
};

// Eight frames in Sonic 3 mapping format (Mappings.h): frame n is one 2x2 piece at (-8,-8) with tile n
#define FRAME(n) 0x00, 0x01, 0xF8, 0x05, 0x00, (n), 0xFF, 0xF8
static const uint8_t mappings[] = {
    0x00, 0x10, 0x00, 0x18, 0x00, 0x20, 0x00, 0x28, 0x00, 0x30, 0x00, 0x38, 0x00, 0x40, 0x00, 0x48,
    FRAME(0), FRAME(1), FRAME(2), FRAME(3), FRAME(4), FRAME(5), FRAME(6), FRAME(7),
};

static int16_t cam_x, cam_y;
static SpriteView view;

static void Reset(void) {
    Rings_SetLook(mappings, 0);
    Rings_Init(layout);
    rings_frame = 0;
    cam_x = cam_y = 0;
    view = (SpriteView){{ {NULL, NULL}, {&cam_x, &cam_y}, {&cam_x, &cam_y}, {&cam_x, &cam_y} }};
    level_screen_y_wrap = 0x7FF;
    memset(screen1p.sprite_table, 0, (BUFFER_SPRITES + 1) * sizeof(sprite_t));
}

// How many sprites Rings_Render puts for the view, into screen1p.sprite_table
static int Render(void) {
    sprite_t *sprite = screen1p.sprite_table;
    uint8_t sprite_i = 0;
    Rings_Render(&sprite, &sprite_i, &view, 0x80, 320);
    return sprite_i;
}

static void Rings_CountsTheLayoutsRings(void) {
    Reset();
    CHECK_EQ(Rings_Count(), 6);
    int16_t x, y;
    Rings_Position(4, &x, &y);
    CHECK_EQ(x, 0x200);
    CHECK_EQ(y, 0x98);
    Rings_Init(NULL);
    CHECK_EQ(Rings_Count(), 0);
}

static void Rings_ThePlayerCollectsTheRingsInTheirBox(void) {
    Reset();
    CHECK_EQ(Rings_Collide(0x100, 0x40, 0x13), 1); // on the first ring only: the next is $18 away
    CHECK_EQ(Rings_StatusOf(0).frame, 4);
    CHECK_EQ(Rings_StatusOf(0).timer, 6);
    CHECK_EQ(Rings_StatusOf(1).frame, 0);
    CHECK_EQ(Rings_Collide(0x100, 0x40, 0x13), 0); // collected: not again
    CHECK_EQ(Rings_Collide(0x400, 0x40, 0x13), 0); // nothing near

    CHECK_EQ(Rings_Collide(0x10C, 0x40, 0x13), 1); // between the second and the third ring: only the second is inside the box ($10 wide, from 8 left of the player)
    CHECK_EQ(Rings_StatusOf(1).frame, 4);
    CHECK_EQ(Rings_StatusOf(2).frame, 0);

    CHECK_EQ(Rings_Collide(0x200, 0x8C, 0x13), 2); // a tall box takes the column of two
}

static void Rings_TheBoxIsAsHighAsThePlayer(void) {
    Reset();
    CHECK_EQ(Rings_Collide(0x100, 0x40 + 0x20, 0x13), 0); // 32 pixels below: out of the 32 high box? (the box runs from 16 above to 16 below the player, less the ring's 6)
    CHECK_EQ(Rings_Collide(0x100, 0x40 + 0x0A, 0x13), 1);
    Reset();
    CHECK_EQ(Rings_Collide(0x100, 0x40 - 0x0A, 0x0E), 1); // a rolling player (radius 14) has a smaller box, but still this
    Reset();
    CHECK_EQ(Rings_Collide(0x100, 0x40 - 0x14, 0x0E), 0);
}

static void Rings_ACollectedRingSparklesThenIsGone(void) {
    Reset();
    Rings_Collide(0x100, 0x40, 0x13);
    CHECK_EQ(Rings_ConsumingCount(), 1);
    for (int i = 0; i < 5; i++)
        Rings_Update();
    CHECK_EQ(Rings_StatusOf(0).frame, 4); // 6 frames to a frame
    Rings_Update();
    CHECK_EQ(Rings_StatusOf(0).frame, 5);
    for (int i = 0; i < 6 * 2; i++)
        Rings_Update();
    CHECK_EQ(Rings_StatusOf(0).frame, 7);
    for (int i = 0; i < 5; i++)
        Rings_Update();
    CHECK_EQ(Rings_StatusOf(0).frame, 7);
    Rings_Update();
    CHECK_EQ(Rings_StatusOf(0).frame, 0xFF); // gone
    CHECK_EQ(Rings_ConsumingCount(), 0);
    CHECK_EQ(Rings_Collide(0x100, 0x40, 0x13), 0);
}

static void Rings_StartingTheLevelBringsThemBack(void) {
    Reset();
    Rings_Collide(0x100, 0x40, 0x13);
    Rings_Init(layout);
    CHECK_EQ(Rings_StatusOf(0).frame, 0);
    CHECK_EQ(Rings_ConsumingCount(), 0);

    Rings_Collide(0x100, 0x40, 0x13);
    Sst_Reset(); // (whatever clears the level's objects clears its rings)
    CHECK_EQ(Rings_Count(), 0);
}

static void Rings_TheViewDrawsTheRingsNearIt(void) {
    Reset();
    cam_x = 0xF0;
    CHECK_EQ(Render(), 5); // rings 0 to 4: the far one is past the view's right edge
    CHECK_EQ(screen1p.sprite_table[0].x, 0x100 - 0xF0 + 0x80 - 8);
    CHECK_EQ(screen1p.sprite_table[0].y, 0x40 + 0x80 - 8);
    CHECK_EQ(screen1p.sprite_table[0].pattern, 0); // rings_frame 0

    rings_frame = 2;
    Render();
    CHECK_EQ(screen1p.sprite_table[0].pattern, 2);

    cam_x = 0x208;
    CHECK_EQ(Render(), 2); // only the two at $200: the view takes the rings from 8 pixels left of the camera
}

static void Rings_TheViewDrawsTheSparkleAndSkipsWhatIsGone(void) {
    Reset();
    cam_x = 0xF0;
    Rings_Collide(0x100, 0x40, 0x13);
    CHECK_EQ(Render(), 5);
    CHECK_EQ(screen1p.sprite_table[0].pattern, 4); // the sparkle's first frame

    for (int i = 0; i < 6 * 4; i++)
        Rings_Update();
    CHECK_EQ(Render(), 4); // the collected ring is gone
}

static void Rings_TheViewsHeightAndTheLevelsWrapDecideWhatIsDrawn(void) {
    Reset();
    cam_x = 0xF0;
    cam_y = 0x200; // the rings are 400 pixels above the view
    CHECK_EQ(Render(), 0);

    cam_y = 0x48;
    CHECK_EQ(Render(), 5); // the rings at y $40 are 8 pixels above the view's top: still drawn
    cam_y = 0x49;
    CHECK_EQ(Render(), 2); // 9 above: not drawn (the two lower ones are)

    level_screen_y_wrap = 0xFF; // a level 256 pixels high wraps: its rings are also seen from a view that starts a level's height below them
    cam_y = 0x140;
    CHECK_EQ(Render(), 5);
}

static void Rings_ASecondViewHasItsOwnRings(void) {
    Reset();
    static int16_t cam2_x, cam2_y;
    SpriteView view2 = {{ {NULL, NULL}, {&cam2_x, &cam2_y}, {&cam2_x, &cam2_y}, {&cam2_x, &cam2_y} }};
    cam_x = 0;
    cam2_x = 0x7F0; // the far ring
    cam2_y = 0;
    sprite_t *sprite = screen1p.sprite_table;
    uint8_t sprite_i = 0;
    Rings_Render(&sprite, &sprite_i, &view2, 0x80, 320);
    CHECK_EQ(sprite_i, 1);
    CHECK_EQ(screen1p.sprite_table[0].x, 0x800 - 0x7F0 + 0x80 - 8);
}

// Every ring layout the game has (Sonic 1's, or Sonic 2's): sorted by X, no more rings than the status table holds, each reached by the handler
static void Rings_EveryLevelsLayoutIsSortedAndFits(void) {
    int checked = 0;
    for (int zone = 0; zone < ZoneId_Num; zone++) {
        for (int act = 0; act < 4; act++) {
            const uint8_t *rings = level_ring[zone][act];
            if (rings == NULL)
                continue;
            Rings_Init(rings);
            const int n = Rings_Count();
            CHECK(n <= RINGS_MAX);
            int16_t last_x = 0, x, y;
            for (int i = 0; i < n; i++) {
                Rings_Position(i, &x, &y);
                CHECK(x >= last_x);
                last_x = x;
            }
            // the end mark is where the handler stopped
            CHECK_EQ(rings[n * 4], 0xFF);
            CHECK_EQ(rings[n * 4 + 1], 0xFF);
            checked++;
        }
    }
    CHECK(checked > 10);
    Rings_Init(NULL);
}

void RegisterRingsTests(void) {
    RUN_TEST(Rings_CountsTheLayoutsRings);
    RUN_TEST(Rings_ThePlayerCollectsTheRingsInTheirBox);
    RUN_TEST(Rings_TheBoxIsAsHighAsThePlayer);
    RUN_TEST(Rings_ACollectedRingSparklesThenIsGone);
    RUN_TEST(Rings_StartingTheLevelBringsThemBack);
    RUN_TEST(Rings_TheViewDrawsTheRingsNearIt);
    RUN_TEST(Rings_TheViewDrawsTheSparkleAndSkipsWhatIsGone);
    RUN_TEST(Rings_TheViewsHeightAndTheLevelsWrapDecideWhatIsDrawn);
    RUN_TEST(Rings_ASecondViewHasItsOwnRings);
    RUN_TEST(Rings_EveryLevelsLayoutIsSortedAndFits);
}
