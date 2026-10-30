#include "test.h"

#include <string.h>

#include "Sst.h"

// The status table of Sonic 3 & Knuckles and its handler (engine/Sst.c), checked against the routines of sonic3k.asm they come from.

static int ran[SST_COUNT];
static int order[SST_COUNT], order_n;

static void Counter(Sst *self) {
    int i = (int)(self - sst);
    ran[i]++;
    order[order_n++] = i;
}

static void Fresh(void) {
    Sst_Reset();
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

void RegisterSstTests(void) {
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
}
