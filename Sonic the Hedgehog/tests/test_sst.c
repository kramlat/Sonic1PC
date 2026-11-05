#include "test.h"

#include <string.h>

#include "Sst.h"
#include "Camera.h"
#include "EngineObject.h"
#include "Video.h"
#include "Viewport.h"

// The status table of Sonic 3 & Knuckles and its handler (engine/Sst.c), checked against the routines of sonic3k.asm they come from.

static int ran[SST_COUNT];
static int order[SST_COUNT], order_n;

static void Counter(Sst *self) {
    int i = (int)(self - sst);
    ran[i]++;
    order[order_n++] = i;
}

static void Fresh(void) {
    Sst_ResetAll();
    memset(ran, 0, sizeof(ran));
    order_n = 0;
}

static void Sst_ProcessRunsEverySlotInOrderAndSkipsFreeOnes(void) {
    Fresh();
    sst[5].code = Counter;
    sst[0].code = Counter;
    sst[SST_COUNT - 1].code = Counter;
    Sst_Process(false);
    CHECK_EQ(order_n, 3);
    CHECK_EQ(order[0], 0);
    CHECK_EQ(order[1], 5);
    CHECK_EQ(order[2], SST_COUNT - 1);
}

static void Sst_AFrozenLevelOnlyDrawsItsOnScreenObjects(void) {
    Fresh();
    sst[SST_FIRST_DYNAMIC + 3].code = Counter;
    sst[SST_FIRST_LEVEL + 2].code = Counter;
    sst[SST_FIRST_LEVEL + 2].render_flags = SST_RENDER_ON_SCREEN;
    sst[SST_FIRST_LEVEL + 3].code = Counter;
    Sst_Process(true);
    CHECK_EQ(ran[SST_FIRST_DYNAMIC + 3], 1);   // (dynamic objects still run)
    CHECK_EQ(ran[SST_FIRST_LEVEL + 2], 0);     // (the level's do not)
    CHECK_EQ(ran[SST_FIRST_LEVEL + 3], 0);
    CHECK_EQ(Sst_QueueCount(0), 1);            // (the one that was on screen is drawn)
    CHECK(Sst_QueueEntry(0, 0) == &sst[SST_FIRST_LEVEL + 2]);
}

static void Sst_DrawQueuesByPriorityAndPassesOnWhenFull(void) {
    Fresh();
    sst[1].priority = 0x80 * 3;
    sst[2].priority = 0x80 * 3;
    sst[3].priority = 0;
    Sst_Draw(&sst[1]);
    Sst_Draw(&sst[2]);
    Sst_Draw(&sst[3]);
    CHECK_EQ(Sst_QueueCount(3), 2);
    CHECK(Sst_QueueEntry(3, 0) == &sst[1]);
    CHECK(Sst_QueueEntry(3, 1) == &sst[2]);
    CHECK_EQ(Sst_QueueCount(0), 1);
    // a queue holds 63: the 64th goes to the next
    Sst_ClearQueues();
    for (int i = 0; i < SST_QUEUE_SIZE + 1; i++)
        Sst_Draw(&sst[7]);
    CHECK_EQ(Sst_QueueCount(0), SST_QUEUE_SIZE);
    CHECK_EQ(Sst_QueueCount(1), 1);
    // and the last queue drops what does not fit
    Sst_ClearQueues();
    sst[7].priority = 0x80 * 7;
    for (int i = 0; i < SST_QUEUE_SIZE + 5; i++)
        Sst_Draw(&sst[7]);
    CHECK_EQ(Sst_QueueCount(7), SST_QUEUE_SIZE);
}

static void Sst_DeleteFreesAndClearsTheSlot(void) {
    Fresh();
    sst[9].code = Counter;
    sst[9].x_pos.f.u = 123;
    sst[9].own.u8[5] = 7;
    Sst_Delete(&sst[9]);
    CHECK(sst[9].code == NULL);
    CHECK_EQ(sst[9].x_pos.f.u, 0);
    CHECK_EQ(sst[9].own.u8[5], 0);
}

static void Sst_AllocateSearchesAsTheOriginalDoes(void) {
    Fresh();
    // it starts at the second dynamic slot, and runs one slot past the last dynamic one
    Sst *first = Sst_Allocate();
    CHECK(first == &sst[SST_FIRST_DYNAMIC + 1]);
    for (int i = SST_FIRST_DYNAMIC; i < SST_FIRST_LEVEL; i++)
        sst[i].code = Counter;
    CHECK(Sst_Allocate() == &sst[SST_FIRST_LEVEL]);
    sst[SST_FIRST_LEVEL].code = Counter;
    CHECK(Sst_Allocate() == NULL);
    // after the current one, within the dynamic slots
    Fresh();
    sst[SST_FIRST_DYNAMIC + 4].code = Counter;
    CHECK(Sst_AllocateAfter(&sst[SST_FIRST_DYNAMIC + 3]) == &sst[SST_FIRST_DYNAMIC + 5]);
    CHECK(Sst_AllocateAfter(&sst[SST_FIRST_LEVEL - 1]) == NULL);
}

static void Sst_MoveSpriteAddsVelocityAndGravity(void) {
    Fresh();
    Sst *o = &sst[5];
    o->x_vel = 0x180;                 // a pixel and a half a frame
    o->y_vel = -0x100;
    Sst_MoveSprite(o);
    CHECK_EQ(o->x_pos.v, 0x180 * 256);
    CHECK_EQ(o->y_pos.v, -0x100 * 256);
    CHECK_EQ(o->y_vel, -0x100 + 0x38);  // (gravity is added after the old speed is used)
    Sst_MoveSprite2(o);
    CHECK_EQ(o->x_pos.v, 0x300 * 256);
    CHECK_EQ(o->y_vel, -0x100 + 0x38);  // (no gravity)
}

static void Sst_ReversedGravityMovesTheOtherWay(void) {
    Fresh();
    Sst *o = &sst[5];
    o->y_vel = 0x200;
    sst_reverse_gravity = 0;
    Sst_MoveSpriteTestGravity(o);
    CHECK_EQ(o->y_pos.v, 0x200 * 256);
    o->y_pos.v = 0;
    o->y_vel = 0x200;
    sst_reverse_gravity = 1;
    Sst_MoveSpriteTestGravity(o);
    CHECK_EQ(o->y_pos.v, -0x200 * 256);
    CHECK_EQ(o->y_vel, 0x200 + 0x38);   // (gravity is added either way)
    o->y_pos.v = 0;
    Sst_MoveSpriteTestGravity2(o);
    CHECK_EQ(o->y_pos.v, -(0x200 + 0x38) * 256);
    sst_reverse_gravity = 0;
}

// Two animations: 0 shows frames 1, 2, 3 with a delay of 2 and loops; 1 shows 5, 6, then goes to animation 0; 2 shows 7 then the routine counter on; 3 loops back by 2
static const uint8_t scripts[] = {
    0x00, 0x08, 0x00, 0x0D, 0x00, 0x12, 0x00, 0x15,       // the offsets of the four scripts
    0x02, 1, 2, 3, 0xFF,                                   // 8: delay 2, frames, loop
    0x00, 5, 6, 0xFD, 0,                                   // $D: delay 0, 5, 6, then animation 0
    0x00, 7, 0xFC,                                         // $12
    0x00, 1, 2, 0xFE, 2, 0,                                // $15: 1, 2, back by 2 (to 1)
};

static void Sst_AnimateWalksTheScript(void) {
    Fresh();
    Sst *o = &sst[5];
    o->anim = 0;
    o->prev_anim = 0;
    o->status = SST_STATUS_X_FLIP;      // (the flips of the status go into the render flags)
    o->render_flags = 0x80 | 0x04;
    o->anim_frame_timer = 0;
    int seen[8], n = 0;
    for (int f = 0; f < 8; f++) {       // a frame changes every third call: the timer runs 2, 1, 0 and then borrows
        Sst_Animate(o, scripts);
        if (f % 3 == 0)
            seen[n++] = o->mapping_frame;
    }
    CHECK_EQ(seen[0], 1);
    CHECK_EQ(seen[1], 2);
    CHECK_EQ(seen[2], 3);
    CHECK_EQ(o->render_flags & 3, SST_STATUS_X_FLIP);
    CHECK_EQ(o->render_flags & 0xFC, 0x84); // (the other bits stay)
}

// A Sonic 1 / Sonic 2 script: the frame in 5 bits, $20 flips it in X and $40 in Y against the status (their AnimateSprite); Sst_Animate leaves the byte alone
static const uint8_t flip_scripts[] = {
    0x00, 0x02,
    0x00, 1, 0x22, 0x43, 0x64, 0xFF, // frames 1, 2 flipped in X, 3 flipped in Y, 4 flipped both ways
};

static void Sst_AnimateFlipsTakesTheFlipBitsOutOfTheFrame(void) {
    Fresh();
    Sst *o = &sst[5];
    o->anim = o->prev_anim = 0;
    o->status = 0;
    o->render_flags = 0x80;
    o->anim_frame_timer = 0;
    Sst_AnimateFlips(o, flip_scripts);
    CHECK_EQ(o->mapping_frame, 1);
    CHECK_EQ(o->render_flags & 3, 0);
    Sst_AnimateFlips(o, flip_scripts);
    CHECK_EQ(o->mapping_frame, 2);
    CHECK_EQ(o->render_flags & 3, 1);
    Sst_AnimateFlips(o, flip_scripts);
    CHECK_EQ(o->mapping_frame, 3);
    CHECK_EQ(o->render_flags & 3, 2);
    o->status = SST_STATUS_X_FLIP; // (against the status: the object's own flip cancels the frame's)
    Sst_AnimateFlips(o, flip_scripts);
    CHECK_EQ(o->mapping_frame, 4);
    CHECK_EQ(o->render_flags & 3, 2);
    CHECK_EQ(o->render_flags & 0x80, 0x80);
    // the Sonic 3 & Knuckles one does not: the byte is the frame
    o->anim_frame = 1;
    o->anim_frame_timer = 0;
    Sst_Animate(o, flip_scripts);
    CHECK_EQ(o->mapping_frame, 0x22);
}

static void Sst_AnimateCodes(void) {
    Fresh();
    Sst *o = &sst[5];
    // $FF: the script loops
    o->anim = 0; o->prev_anim = 0;
    for (int i = 0; i < 12; i++) Sst_Animate(o, scripts);
    CHECK(o->mapping_frame >= 1 && o->mapping_frame <= 3);
    // $FD: another animation
    Fresh();
    o->anim = 1; o->prev_anim = 1; o->anim_frame = 0; o->anim_frame_timer = 0;
    Sst_Animate(o, scripts); CHECK_EQ(o->mapping_frame, 5);
    Sst_Animate(o, scripts); CHECK_EQ(o->mapping_frame, 6);
    Sst_Animate(o, scripts); CHECK_EQ(o->anim, 0);
    // $FC: the routine on by 2
    Fresh();
    o->anim = 2; o->prev_anim = 2; o->routine = 4;
    Sst_Animate(o, scripts); CHECK_EQ(o->mapping_frame, 7);
    Sst_Animate(o, scripts); CHECK_EQ(o->routine, 6);
    // $FE: back by the byte after it
    Fresh();
    o->anim = 3; o->prev_anim = 3;
    Sst_Animate(o, scripts); CHECK_EQ(o->mapping_frame, 1);
    Sst_Animate(o, scripts); CHECK_EQ(o->mapping_frame, 2);
    Sst_Animate(o, scripts); CHECK_EQ(o->mapping_frame, 1);
    Sst_Animate(o, scripts); CHECK_EQ(o->mapping_frame, 2);
}

// Irregular delay: pairs of frame and delay: animation 0 is (1, 0) (2, 3) (3, 0) and loops
static const uint8_t irregular[] = {
    0x00, 0x02,
    1, 0, 2, 3, 3, 0, 0xFF, 0,
};

static void Sst_AnimateIrregularUsesEachFramesOwnDelay(void) {
    Fresh();
    Sst *o = &sst[5];
    o->anim = 0; o->prev_anim = 0;
    Sst_AnimateIrregular(o, irregular); CHECK_EQ(o->mapping_frame, 1);
    Sst_AnimateIrregular(o, irregular); CHECK_EQ(o->mapping_frame, 2);
    CHECK_EQ(o->anim_frame_timer, 3);
    for (int i = 0; i < 3; i++) Sst_AnimateIrregular(o, irregular); // (the delay runs out)
    CHECK_EQ(o->mapping_frame, 2);
    Sst_AnimateIrregular(o, irregular); CHECK_EQ(o->mapping_frame, 3);
    Sst_AnimateIrregular(o, irregular); CHECK_EQ(o->mapping_frame, 1); // (looped)
}

// ---- rendering: Render_Sprites for the status table ----

// Sonic 3 mappings (6-byte pieces): frame 0 has no pieces, frame 1 one 2x2 piece at (-8,-8) with tile 1, frame 2 one 1x1 piece at (4,0) with tile 2
static const uint8_t s3_mappings[] = {
    0x00, 0x06, 0x00, 0x08, 0x00, 0x10,
    0x00, 0x00,
    0x00, 0x01, 0xF8, 0x05, 0x00, 0x01, 0xFF, 0xF8,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x04,
};

static void PrepareRender(void) {
    Sst_Reset();
    memset(screen1p.sprite_table, 0, (BUFFER_SPRITES + 1) * sizeof(sprite_t));
    scrpos_x.f.u = 0;
    scrpos_y.f.u = 0;
}

static Sst *Placed(int slot, int x, int y, int frame) {
    Sst *o = &sst[slot];
    o->code = Counter;
    o->mappings = s3_mappings;
    o->art_tile = 0x100;
    o->x_pos.f.u = (int16_t)x;
    o->y_pos.f.u = (int16_t)y;
    o->mapping_frame = (uint8_t)frame;
    o->width_pixels = 16;
    o->height_pixels = 16;
    o->render_flags = SST_RENDER_LEVEL;
    return o;
}

static void Sst_RenderPlacesAPieceAgainstTheCamera(void) {
    PrepareRender();
    scrpos_x.f.u = 40;
    scrpos_y.f.u = 20;
    Sst *o = Placed(5, 100, 100, 1);
    Sst_Draw(o);
    BuildSprites(NULL);
    CHECK_EQ(sprite_count, 1);
    CHECK_EQ(screen1p.sprite_table[0].x, 128 + 100 - 40 - 8);
    CHECK_EQ(screen1p.sprite_table[0].y, 128 + 100 - 20 - 8);
    CHECK_EQ(screen1p.sprite_table[0].pattern, 0x101);
    CHECK(o->render_flags & SST_RENDER_ON_SCREEN);
    CHECK_EQ(Sst_QueueCount(0), 0); // (the queue is empty for the next frame)
}

static void Sst_RenderMirrorsAboutTheOriginForFlips(void) {
    PrepareRender();
    Sst *o = Placed(5, 100, 100, 2);
    Sst_Draw(o);
    BuildSprites(NULL);
    const uint16_t plain_x = screen1p.sprite_table[0].x;
    CHECK_EQ(plain_x, 128 + 100 + 4);
    PrepareRender();
    o = Placed(5, 100, 100, 2);
    o->render_flags |= SST_RENDER_X_FLIP;
    Sst_Draw(o);
    BuildSprites(NULL);
    CHECK_EQ(screen1p.sprite_table[0].x, 128 + 100 - 4 - 8);
}

static void Sst_RenderLeavesOutWhatIsOffScreen(void) {
    PrepareRender();
    Sst *o = Placed(5, -100, 100, 1);
    o->render_flags |= SST_RENDER_ON_SCREEN;
    Sst_Draw(o);
    Sst *p = Placed(6, 100, 100 + 600, 1); // (far below)
    Sst_Draw(p);
    BuildSprites(NULL);
    CHECK_EQ(sprite_count, 0);
    CHECK(!(o->render_flags & SST_RENDER_ON_SCREEN)); // (cleared)
}

static void Sst_RenderUsesScreenPositionsWithoutTheLevelFlag(void) {
    PrepareRender();
    scrpos_x.f.u = 500;
    Sst *o = Placed(5, 128 + 50, 128 + 60, 2);
    o->render_flags = 0;
    Sst_Draw(o);
    BuildSprites(NULL);
    CHECK_EQ(sprite_count, 1);
    CHECK_EQ(screen1p.sprite_table[0].x, 128 + 50 + 4);
    CHECK_EQ(screen1p.sprite_table[0].y, 128 + 60);
}

static void Sst_RenderDrawsAMultiDrawObjectsChildrenAndCullsTheirPieces(void) {
    PrepareRender();
    Sst *o = Placed(5, 100, 100, 1);
    o->render_flags |= SST_RENDER_MULTI_DRAW;
    o->child_count = 3;
    o->children[0] = (SstChild){ 120, 100, 2 };
    o->children[1] = (SstChild){ 80, 110, 1 };
    o->children[2] = (SstChild){ 900, 100, 2 }; // (a piece far to the side: not drawn)
    Sst_Draw(o);
    BuildSprites(NULL);
    CHECK_EQ(sprite_count, 3);
    CHECK_EQ(screen1p.sprite_table[1].x, 128 + 120 + 4);
}

static void Sst_RenderStaticMappingsAreOneLonePiece(void) {
    PrepareRender();
    Sst *o = Placed(5, 100, 100, 0);
    o->mappings = s3_mappings + 0x12; // (frame 2's piece, by itself)
    o->render_flags |= SST_RENDER_STATIC;
    Sst_Draw(o);
    BuildSprites(NULL);
    CHECK_EQ(sprite_count, 1);
    CHECK_EQ(screen1p.sprite_table[0].pattern, 0x102);
}

void RegisterSstTests(void) {
    RUN_TEST(Sst_AnimateFlipsTakesTheFlipBitsOutOfTheFrame);
    RUN_TEST(Sst_ProcessRunsEverySlotInOrderAndSkipsFreeOnes);
    RUN_TEST(Sst_AFrozenLevelOnlyDrawsItsOnScreenObjects);
    RUN_TEST(Sst_DrawQueuesByPriorityAndPassesOnWhenFull);
    RUN_TEST(Sst_DeleteFreesAndClearsTheSlot);
    RUN_TEST(Sst_AllocateSearchesAsTheOriginalDoes);
    RUN_TEST(Sst_MoveSpriteAddsVelocityAndGravity);
    RUN_TEST(Sst_ReversedGravityMovesTheOtherWay);
    RUN_TEST(Sst_AnimateWalksTheScript);
    RUN_TEST(Sst_AnimateCodes);
    RUN_TEST(Sst_AnimateIrregularUsesEachFramesOwnDelay);
    RUN_TEST(Sst_RenderPlacesAPieceAgainstTheCamera);
    RUN_TEST(Sst_RenderMirrorsAboutTheOriginForFlips);
    RUN_TEST(Sst_RenderLeavesOutWhatIsOffScreen);
    RUN_TEST(Sst_RenderUsesScreenPositionsWithoutTheLevelFlag);
    RUN_TEST(Sst_RenderDrawsAMultiDrawObjectsChildrenAndCullsTheirPieces);
    RUN_TEST(Sst_RenderStaticMappingsAreOneLonePiece);
}
