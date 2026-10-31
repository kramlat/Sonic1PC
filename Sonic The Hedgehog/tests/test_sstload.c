#include "test.h"

#include <string.h>

#include "SstLoad.h"

// The loader of the status table (engine/SstLoad.c), checked against Load_Sprites of sonic3k.asm on a made-up layout.

#define X(v) (uint8_t)((v) >> 8), (uint8_t)(v)
#define ENTRY(x, y, id, sub) X(x), X(y), (id), (sub)

static void CodeA(Sst *self) { (void)self; }
static void CodeB(Sst *self) { (void)self; }

static const SstCode codes[8] = {NULL, CodeA, CodeB, CodeA, NULL, CodeA, CodeA, CodeA};
static const SstObjectIndex index_ = {codes, 8};

// 0 and 1 are in the first window; 2 is flipped (x), has a subtype; 3 is far below; 4 is far below but loads whatever the Y; 5 has an id the table has no routine for; 6, 7 are far to the right
static const uint8_t layout[] = {
    ENTRY(0x100, 0x100, 1, 0x0A), ENTRY(0x200, 0x100, 1, 0), ENTRY(0x300, 0x100 | 0x2000, 2, 5), ENTRY(0x400, 0x900, 1, 0),
    ENTRY(0x500, 0x900 | 0x8000, 1, 0), ENTRY(0x600, 0x100, 4, 0), ENTRY(0x1000, 0x100, 3, 0), ENTRY(0x1100, 0x100, 3, 0),
    X(0xFFFF), 0, 0, 0, 0,
};

static SstLoadCamera Cam(int16_t x, int16_t y) {
    SstLoadCamera c = {x, y, false, 0xFFF};
    return c;
}

static void Start(const uint8_t *entries, int16_t x, int16_t y) {
    Sst_Reset();
    SstLoadCamera c = Cam(x, y);
    SstLoad_Init(&index_, entries, false, &c);
}

static void Move(int16_t x, int16_t y) {
    SstLoadCamera c = Cam(x, y);
    SstLoad_Update(&c);
}

static Sst *Of(int entry) {
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code != NULL && sst[i].respawn == entry + 1)
            return &sst[i];
    return NULL;
}

static int Count(void) {
    int n = 0;
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code != NULL)
            n++;
    return n;
}

static void SstLoad_StartLoadsTheWindowOfTheCamera(void) {
    Start(layout, 0, 0);
    // X 0 to 0x280 ahead, Y 0 to 0x200 below
    CHECK(Of(0) != NULL);
    CHECK(Of(1) != NULL);
    CHECK(Of(2) == NULL);
    CHECK_EQ(Count(), 2);
    CHECK_EQ(Of(0)->x_pos.f.u, 0x100);
    CHECK_EQ(Of(0)->y_pos.f.u, 0x100);
    CHECK_EQ(Of(0)->subtype, 0x0A);
    CHECK(Of(0)->code == CodeA);
    CHECK(sst_respawn_table[0] & 0x80);
    CHECK(!(sst_respawn_table[2] & 0x80));
    CHECK_EQ(SstLoad_CoarseBackX(), (int16_t)0xFF80);
}

static void SstLoad_MovingRightLoadsWhatComesInAndTakesTheFlipsFromTheEntry(void) {
    Start(layout, 0, 0);
    Move(0x180, 0);
    Sst *o = Of(2);
    CHECK(o != NULL);
    CHECK(o->code == CodeB);
    CHECK_EQ(o->subtype, 5);
    CHECK_EQ(o->render_flags, 1);
    CHECK_EQ(o->status, 1);
    CHECK_EQ(o->y_pos.f.u, 0x100); // (the flip bits are not in the Y)
    CHECK(Of(3) == NULL);
    CHECK_EQ(Count(), 3);
}

static void SstLoad_AColumnThatIsNotNewLoadsNothing(void) {
    Start(layout, 0, 0);
    Move(0x180, 0);
    int n = Count();
    Move(0x1FF, 0); // (the same 128-pixel column)
    CHECK_EQ(Count(), n);
}

static void SstLoad_TheYWindowKeepsOutWhatIsFarBelowUnlessBit15Says(void) {
    Start(layout, 0, 0);
    Move(0x180, 0);
    Move(0x280, 0);
    CHECK(Of(3) == NULL);       // x 0x400, y 0x900: outside the window
    Move(0x300, 0);
    CHECK(Of(4) != NULL);       // bit 15: loads whatever the Y
    CHECK_EQ(Of(4)->y_pos.f.u, 0x900);
    CHECK(Of(3) == NULL);
}

static void SstLoad_AnIdWithNoRoutineIsLeftAlone(void) {
    Start(layout, 0, 0);
    Move(0x300, 0);
    Move(0x400, 0);
    CHECK(Of(5) == NULL);
    CHECK(!SstLoad_Handles(4));
    CHECK(!SstLoad_Handles(200));
    CHECK(SstLoad_Handles(1));
    CHECK(!(sst_respawn_table[5] & 0x80));
}

static void SstLoad_MovingDownLoadsTheRowThatComesIn(void) {
    Start(layout, 0, 0);
    Move(0x180, 0);
    Move(0x280, 0);
    Move(0x280, 0x780);   // the strip 0x900 to 0x980 is the new bottom row
    Sst *o = Of(3);
    CHECK(o != NULL);
    CHECK_EQ(o->y_pos.f.u, 0x900);
}

static void SstLoad_MovingDownLoadsNothingPastTheLevelsWrap(void) {
    Start(layout, 0, 0);
    Move(0x180, 0);
    Move(0x280, 0);
    SstLoadCamera c = {0x280, 0x780, false, 0x7FF};   // (a 0x800-high level: 0x780 + 0x180 is past it)
    SstLoad_Update(&c);
    CHECK(Of(3) == NULL);
}

static void SstLoad_AnObjectThatLeavesRangeForgetsItsPlaceAndComesBack(void) {
    Start(layout, 0, 0);
    Move(0x180, 0);
    Move(0x300, 0);       // the coarse back edge is 0x280 now
    Sst *o = Of(1);       // x 0x200
    CHECK(o != NULL);
    int slot = (int)(o - sst);
    SstOnScreenTest(o);
    CHECK(sst[slot].code == NULL);            // deleted
    CHECK(!(sst_respawn_table[1] & 0x80));    // and forgotten
    Move(0x180, 0);                           // moving left, the back edge loads it again
    CHECK(Of(1) != NULL);
}

static void SstLoad_AnObjectDestroyedForGoodIsNotLoadedAgain(void) {
    Start(layout, 0, 0);
    Move(0x180, 0);
    Move(0x300, 0);
    Sst *o = Of(1);
    Sst_Delete(o);                            // (no forgetting)
    Move(0x180, 0);
    CHECK(Of(1) == NULL);
    CHECK(sst_respawn_table[1] & 0x80);
}

static void SstLoad_OnScreenTestDrawsWhatIsNearAndDeletesWhatIsFar(void) {
    Start(layout, 0, 0);
    Sst *o = Of(0);
    int slot = (int)(o - sst);
    Sst_ClearQueues();
    SstOnScreenTest(o);
    CHECK_EQ(Sst_QueueCount(0), 1);
    CHECK(sst[slot].code != NULL);
    sst[slot].x_pos.f.u = 0x1000;             // more than 0x280 past the coarse back edge
    SstOnScreenTest(&sst[slot]);
    CHECK(sst[slot].code == NULL);
    CHECK(!(sst_respawn_table[0] & 0x80));
}

static void SstLoad_DeleteIfNotInRangeDoesNotDraw(void) {
    Start(layout, 0, 0);
    Sst *o = Of(0);
    Sst_ClearQueues();
    SstDeleteIfNotInRange(o);
    CHECK(Of(0) != NULL);
    CHECK_EQ(Sst_QueueCount(0), 0);
    o->x_pos.f.u = (int16_t)0x2000;
    SstDeleteIfNotInRange(o);
    CHECK(Of(0) == NULL);
}

static void SstLoad_AFullTableStopsAndThePassGoesOnAtTheNextColumn(void) {
    Sst_Reset();
    for (int i = SST_FIRST_DYNAMIC + 1; i <= SST_FIRST_LEVEL; i++)
        sst[i].code = CodeA;
    sst[10].code = NULL;
    sst[20].code = NULL;
    static const uint8_t three[] = {
        ENTRY(0x100, 0x100, 1, 0), ENTRY(0x110, 0x100, 1, 0), ENTRY(0x120, 0x100, 1, 0), X(0xFFFF), 0, 0, 0, 0,
    };
    SstLoadCamera c = Cam(0, 0);
    SstLoad_Init(&index_, three, false, &c);
    CHECK(Of(0) == &sst[10]);                 // (the free slots in order)
    CHECK(Of(1) == &sst[20]);
    CHECK(Of(2) == NULL);
    CHECK(!(sst_respawn_table[2] & 0x80));
    sst[30].code = NULL;
    Move(0x80, 0);
    CHECK(Of(2) == &sst[30]);
}

static void SstLoad_ClearLoadedGivesEveryPlaceBack(void) {
    Start(layout, 0, 0);
    CHECK(sst_respawn_table[0] & 0x80);
    SstLoad_ClearLoaded();
    CHECK(!(sst_respawn_table[0] & 0x80));
    CHECK(!(sst_respawn_table[1] & 0x80));
    CHECK(Of(0) != NULL);   // (the objects stay: only their marks go)
}

static void SstLoad_KeepTableKeepsTheMarks(void) {
    Start(layout, 0, 0);
    Sst_Reset();
    SstLoadCamera c = Cam(0, 0);
    SstLoad_Init(&index_, layout, true, &c);
    CHECK_EQ(Count(), 0);                     // (still marked out there: not loaded again)
}

static void SstLoad_SeekMovesTheWindowWithoutLoading(void) {
    Start(layout, 0, 0);
    int n = Count();
    SstLoad_Seek(0xB00);                      // (the seek reads the camera plus 0x400)
    CHECK_EQ(Count(), n);
    Move(0xC00, 0);
    CHECK_EQ(Count(), n);                     // (the entries seeked over are not loaded: the window is past them)
    CHECK(Of(6) == NULL);
}

static void SstLoad_ALevelWithNoObjectsLoadsNothing(void) {
    Sst_Reset();
    SstLoadCamera c = Cam(0, 0);
    SstLoad_Init(&index_, NULL, false, &c);
    Move(0x400, 0);
    CHECK_EQ(Count(), 0);
}

static void SstLoad_ASecondCameraLoadsItsOwnWindowAndKeepsItsObjectsInRange(void) {
    Start(layout, 0, 0);
    CHECK(Of(6) == NULL);
    CHECK(SstOutOfRange(0x1000));
    SstLoadCamera c2 = Cam(0x1000, 0);
    SstLoad_UpdateSecond(&c2);
    CHECK(Of(6) != NULL);                       // (the entries around the other player's camera)
    CHECK(Of(7) != NULL);
    CHECK(Of(0) != NULL);                       // (the first window's are still there)
    CHECK_EQ(Count(), 4);
    CHECK(!SstOutOfRange(0x1000));              // out of range only of both cameras
    SstLoad_UpdateSecond(&c2);
    CHECK_EQ(Count(), 4);                       // (an object in range of both is made once)
    Move(0, 0);
    CHECK_EQ(Count(), 4);
    SstLoad_EndSecond();
    CHECK(SstOutOfRange(0x1000));
}

void RegisterSstLoadTests(void) {
    RUN_TEST(SstLoad_StartLoadsTheWindowOfTheCamera);
    RUN_TEST(SstLoad_ASecondCameraLoadsItsOwnWindowAndKeepsItsObjectsInRange);
    RUN_TEST(SstLoad_MovingRightLoadsWhatComesInAndTakesTheFlipsFromTheEntry);
    RUN_TEST(SstLoad_AColumnThatIsNotNewLoadsNothing);
    RUN_TEST(SstLoad_TheYWindowKeepsOutWhatIsFarBelowUnlessBit15Says);
    RUN_TEST(SstLoad_AnIdWithNoRoutineIsLeftAlone);
    RUN_TEST(SstLoad_MovingDownLoadsTheRowThatComesIn);
    RUN_TEST(SstLoad_MovingDownLoadsNothingPastTheLevelsWrap);
    RUN_TEST(SstLoad_AnObjectThatLeavesRangeForgetsItsPlaceAndComesBack);
    RUN_TEST(SstLoad_AnObjectDestroyedForGoodIsNotLoadedAgain);
    RUN_TEST(SstLoad_OnScreenTestDrawsWhatIsNearAndDeletesWhatIsFar);
    RUN_TEST(SstLoad_DeleteIfNotInRangeDoesNotDraw);
    RUN_TEST(SstLoad_AFullTableStopsAndThePassGoesOnAtTheNextColumn);
    RUN_TEST(SstLoad_ClearLoadedGivesEveryPlaceBack);
    RUN_TEST(SstLoad_KeepTableKeepsTheMarks);
    RUN_TEST(SstLoad_SeekMovesTheWindowWithoutLoading);
    RUN_TEST(SstLoad_ALevelWithNoObjectsLoadsNothing);
}
