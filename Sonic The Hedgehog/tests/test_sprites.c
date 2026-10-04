#include "test.h"

#include <string.h>

#include "Camera.h"
#include "EngineObject.h"
#include "Video.h"

// The sprite pipeline (engine/Sprites.c): multi-sprite objects and the Y wrap of the on-screen check.

// A table of 3 frames in Sonic 2 mapping format (Mappings.h): frame 0 has no pieces, frame 1 one 2x2 piece at (-8,-8) with tile 1, frame 2 one 1x1 piece at (0,0) with tile 2
static const uint8_t mappings[] = {
    0x00, 0x06, 0x00, 0x08, 0x00, 0x12,                                                // the frame offsets
    0x00, 0x00,                                                                        // frame 0: no pieces
    0x00, 0x01, 0xF8, 0x05, 0x00, 0x01, 0x00, 0x00, 0xFF, 0xF8,                        // frame 1
    0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00,                        // frame 2
};

static Object o;

static void Prepare(void) {
    memset(sprite_buffer, 0, sizeof(sprite_buffer));
    memset(&o, 0, sizeof(o));
    o.type = 1;
    o.mappings = mappings;
    o.tile = 0x100;
    o.priority = 0;
    o.render.f.level_fg = 1;
    o.width_pixels = 16;
    scrpos_x.f.u = 0;
    scrpos_y.f.u = 0;
}

static void Build(void) {
    DisplaySprite(&o);
    BuildSprites(NULL);
}

static void Sprites_MultiSpriteDrawsMainAndChildren(void) {
    Prepare();
    o.render.f.multi_sprite = 1;
    o.pos.l.x.f.u = 100;
    o.pos.l.y.f.u = 100;
    o.frame = 1;
    o.child_count = 2;
    o.children[0] = (ObjectChild){ 120, 100, 2 };
    o.children[1] = (ObjectChild){ 80, 110, 1 };
    Build();

    CHECK_EQ(sprite_count, 3);
    CHECK(o.render.f.on_screen);
    // The main sprite: its piece at (-8,-8) from (128 + 100, 128 + 100)
    CHECK_EQ(sprite_buffer[0][0], 128 + 100 - 8);
    CHECK_EQ(sprite_buffer[0][1], (0x05 << 8) | 1);
    CHECK_EQ(sprite_buffer[0][2], 0x100 + 1);
    CHECK_EQ(sprite_buffer[0][3], 128 + 100 - 8);
    // The first child, frame 2, at (120, 100)
    CHECK_EQ(sprite_buffer[1][0], 128 + 100);
    CHECK_EQ(sprite_buffer[1][1], (0x00 << 8) | 2);
    CHECK_EQ(sprite_buffer[1][2], 0x100 + 2);
    CHECK_EQ(sprite_buffer[1][3], 128 + 120);
    // The second child, frame 1, at (80, 110)
    CHECK_EQ(sprite_buffer[2][0], 128 + 110 - 8);
    CHECK_EQ(sprite_buffer[2][2], 0x100 + 1);
    CHECK_EQ(sprite_buffer[2][3], 128 + 80 - 8);
}

static void Sprites_MainFrameZeroIsNoMainSprite(void) {
    Prepare();
    o.render.f.multi_sprite = 1;
    o.pos.l.x.f.u = 100;
    o.pos.l.y.f.u = 100;
    o.frame = 0; // only the children
    o.child_count = 1;
    o.children[0] = (ObjectChild){ 100, 100, 2 };
    Build();
    CHECK_EQ(sprite_count, 1);
    CHECK(o.render.f.on_screen);
}

static void Sprites_MultiSpriteOutOfViewDrawsNothing(void) {
    Prepare();
    o.render.f.multi_sprite = 1;
    o.pos.l.x.f.u = -500; // far to the left of the camera
    o.pos.l.y.f.u = 100;
    o.frame = 1;
    o.child_count = 1;
    o.children[0] = (ObjectChild){ 100, 100, 2 }; // (even a child that would be in view is not drawn: the main sprite decides)
    Build();
    CHECK_EQ(sprite_count, 0);
    CHECK(!o.render.f.on_screen);
}

static void Sprites_YWrapIsASwitch(void) {
    // An object 0x780 above a camera at 0x7C0: its distance is -0x700 (+0x80), which is 0x100 once it wraps to 11 bits, a place on screen
    int saved = sprite_view.wrap_y;
    Prepare();
    o.frame = 1;
    o.pos.l.x.f.u = 100;
    scrpos_y.f.u = 0x7C0;
    o.pos.l.y.f.u = 0x40;

    sprite_view.wrap_y = 0;
    Build();
    CHECK_EQ(sprite_count, 0);
    CHECK(!o.render.f.on_screen);

    sprite_view.wrap_y = 1;
    Build();
    CHECK_EQ(sprite_count, 1);
    CHECK(o.render.f.on_screen);

    sprite_view.wrap_y = saved;
}

static void Sprites_SplitScreenBuildsTwoTables(void) {
    Object a, b, c;
    memset(sprite_buffer_p2, 0, sizeof(sprite_buffer_p2));
    Prepare();
    a = b = c = o;
    a.tile = b.tile = c.tile = 0x80;
    a.frame = b.frame = c.frame = 1;
    a.pos.l.y.f.u = b.pos.l.y.f.u = c.pos.l.y.f.u = 100;
    a.pos.l.x.f.u = 100;   // in player 1's view (camera 0)
    b.pos.l.x.f.u = 1100;  // in player 2's view (camera 1000)
    c.pos.l.x.f.u = 5000;  // in neither

    sprite_split_screen = SPRITE_SPLIT_STACKED;
    scrpos_x_p2.f.u = 1000;
    scrpos_y_p2.f.u = 0;
    DisplaySprite(&a);
    DisplaySprite(&b);
    DisplaySprite(&c);
    BuildSprites(NULL);
    sprite_split_screen = 0;

    // Player 1's table: a's piece, an ordinary one (the stacked views are two whole pictures, squashed by the VDP)
    CHECK_EQ(sprite_buffer[0][0], 128 + 100 - 8);
    CHECK_EQ(sprite_buffer[0][1], (0x05 << 8) | 1);            // size 5 (2x2 cells); link 1
    CHECK_EQ(sprite_buffer[0][2], 0x81);                          // (the object's tile and the piece's)
    CHECK_EQ(sprite_buffer[0][3], 128 + 100 - 8);
    CHECK_EQ(sprite_buffer[1][0], 0);                          // the list ends
    CHECK_EQ(sprite_buffer[1][1], 0);

    // Player 2's table: b alone, against the second camera
    CHECK_EQ(sprite_buffer_p2[0][0], 128 + 100 - 8);
    CHECK_EQ(sprite_buffer_p2[0][1], (0x05 << 8) | 1);
    CHECK_EQ(sprite_buffer_p2[0][3], 128 + 100 - 8);
    CHECK_EQ(sprite_buffer_p2[1][0], 0);
    CHECK_EQ(sprite_count, 1);

    // On screen means in either view
    CHECK(a.render.f.on_screen);
    CHECK(b.render.f.on_screen);
    CHECK(!c.render.f.on_screen);
}

static void Sprites_Adjust2PArtPointerLeavesTheTile(void) {
    Object obj;
    memset(&obj, 0, sizeof(obj));
    obj.tile = 0x8000 | 0x4000 | 0x0123; // priority, palette 2, tile $123
    sprite_split_screen = 0;
    Object_Adjust2PArtPointer(&obj);
    CHECK_EQ(obj.tile, 0xC123); // not in the split screen: untouched
    sprite_split_screen = SPRITE_SPLIT_SIDE;
    Object_Adjust2PArtPointer(&obj);
    CHECK_EQ(obj.tile, 0xC123); // the side-by-side split uses the art as it is
    sprite_split_screen = SPRITE_SPLIT_STACKED;
    Object_Adjust2PArtPointer(&obj);
    CHECK_EQ(obj.tile, 0xC123); // so does the stacked one (two ordinary views: no halved tiles)
    sprite_split_screen = 0;
}

static void Sprites_SideBySideUsesOrdinaryTilesAndHalfWidthViews(void) {
    Object a, b, d;
    memset(sprite_buffer_p2, 0, sizeof(sprite_buffer_p2));
    Prepare();
    a = b = d = o;
    a.tile = b.tile = d.tile = 0x80;
    a.frame = b.frame = d.frame = 1;
    a.pos.l.y.f.u = b.pos.l.y.f.u = d.pos.l.y.f.u = 100;
    a.pos.l.x.f.u = 100;   // in player 1's half (camera 0, 160 wide)
    d.pos.l.x.f.u = 200;   // past it: the stacked layout would show it, a half-width view does not
    b.pos.l.x.f.u = 1100;  // in player 2's half (camera 1000)

    sprite_split_screen = SPRITE_SPLIT_SIDE;
    scrpos_x_p2.f.u = 1000;
    scrpos_y_p2.f.u = 0;
    DisplaySprite(&a);
    DisplaySprite(&d);
    DisplaySprite(&b);
    BuildSprites(NULL);
    sprite_split_screen = 0;

    // Player 1's table: no masking sprites, the ordinary tile word and size, the usual top
    CHECK_EQ(sprite_buffer[0][0], 128 + 100 - 8);
    CHECK_EQ(sprite_buffer[0][1], (0x05 << 8) | 1);
    CHECK_EQ(sprite_buffer[0][2], 0x80 + 0x0001);
    CHECK_EQ(sprite_buffer[0][3], 128 + 100 - 8);
    CHECK_EQ(sprite_buffer[1][0], 0);                   // d was out of the half-width view
    CHECK(a.render.f.on_screen);
    CHECK(!d.render.f.on_screen);
    // Player 2's table: b, against the second camera, at the same top
    CHECK_EQ(sprite_buffer_p2[0][0], 128 + 100 - 8);
    CHECK_EQ(sprite_buffer_p2[0][1], (0x05 << 8) | 1);
    CHECK_EQ(sprite_buffer_p2[0][3], 128 + 100 - 8);
    CHECK(b.render.f.on_screen);
}

void RegisterSpritesTests(void) {
    RUN_TEST(Sprites_SideBySideUsesOrdinaryTilesAndHalfWidthViews);
    RUN_TEST(Sprites_SplitScreenBuildsTwoTables);
    RUN_TEST(Sprites_Adjust2PArtPointerLeavesTheTile);
    RUN_TEST(Sprites_MultiSpriteDrawsMainAndChildren);
    RUN_TEST(Sprites_MainFrameZeroIsNoMainSprite);
    RUN_TEST(Sprites_MultiSpriteOutOfViewDrawsNothing);
    RUN_TEST(Sprites_YWrapIsASwitch);
}
