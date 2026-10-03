#include "test.h"

#include <string.h>

#include "EngineObject.h"
#include "ObjectsManager.h"

// Sonic 2's objects manager (engine/ObjectsManager.c) on a made-up layout.

#define X(v) (uint8_t)((v) >> 8), (uint8_t)(v)
#define ENTRY(x, y, remember, id, sub) X(x), X((y) | ((remember) ? 0x8000 : 0)), (id), (sub)

// Objects every 0x100 pixels from 0x100 to 0x1000, ids 1..16; the one at 0x400 and the one at 0x800 remember their state.
static const uint8_t layout[] = {
    ENTRY(0x100, 0x10, 0, 1, 0x11), ENTRY(0x200, 0x10, 0, 2, 0x12), ENTRY(0x300, 0x10, 0, 3, 0x13), ENTRY(0x400, 0x10, 1, 4, 0x14),
    ENTRY(0x500, 0x10, 0, 5, 0), ENTRY(0x600, 0x10, 0, 6, 0), ENTRY(0x700, 0x10, 0, 7, 0), ENTRY(0x800, 0x10, 1, 8, 0),
    ENTRY(0x900, 0x10, 0, 9, 0), ENTRY(0xA00, 0x10, 0, 10, 0), ENTRY(0xB00, 0x10, 0, 11, 0), ENTRY(0xC00, 0x10, 0, 12, 0),
    X(0xFFFF), 0, 0, 0, 0,
};

static int Loaded(uint8_t id) {
    int n = 0;
    for (int i = 0; i < LEVEL_OBJECTS; i++)
        if (level_objects[i].type == id)
            n++;
    return n;
}

static ObjectsManager mgr;
static uint8_t marks[0x40];

static void Reset(int16_t camera_x) {
    static const ObjectsManagerConfig config = OBJECTS_MANAGER_DEFAULT_CONFIG;
    memset(objects, 0, sizeof(objects));
    ObjectsManager_Init(&mgr, &config, marks, sizeof(marks), layout, camera_x);
}

static void ObjectsManager_LoadsTheStartWindow(void) {
    Reset(0);
    // Camera 0: everything below 0x280 is loaded (0x100 and 0x200), nothing past it
    CHECK_EQ(Loaded(1), 1);
    CHECK_EQ(Loaded(2), 1);
    CHECK_EQ(Loaded(3), 0);
    CHECK_EQ(mgr.camera_x_coarse, (int16_t)0xFF80); // (0 - 128) rounded to 128, wrapped like the original's word
    // Position, subtype and the flip bits come from the entry
    Object *first = NULL;
    for (int i = 0; i < LEVEL_OBJECTS; i++)
        if (level_objects[i].type == 1)
            first = &level_objects[i];
    CHECK(first != NULL);
    if (first) {
        CHECK_EQ(first->pos.l.x.f.u, 0x100);
        CHECK_EQ(first->pos.l.y.f.u, 0x10);
        CHECK_EQ(OBJECT_SUBTYPE(first), 0x11);
    }
}

static void ObjectsManager_LoadsAheadWhenMovingRight(void) {
    Reset(0);
    // Objects load while their X is below (camera rounded to 128) + 0x280
    ObjectsManager_Update(&mgr, 0x80);
    CHECK_EQ(Loaded(3), 0); // 0x300 is not below 0x80 + 0x280
    ObjectsManager_Update(&mgr, 0x100);
    CHECK_EQ(Loaded(3), 1);
    CHECK_EQ(Loaded(4), 0);
    ObjectsManager_Update(&mgr, 0x200);
    CHECK_EQ(Loaded(4), 1);
    ObjectsManager_Update(&mgr, 0x400);
    CHECK_EQ(Loaded(5), 1);
    CHECK_EQ(Loaded(6), 1);
    ObjectsManager_Update(&mgr, 0x500);
    CHECK_EQ(Loaded(7), 1);
    CHECK_EQ(Loaded(8), 0);
    ObjectsManager_Update(&mgr, 0x580); // the same 128-pixel step as... no, the next one, but 0x800 is still not below 0x580 + 0x280
    CHECK_EQ(Loaded(8), 0);
    ObjectsManager_Update(&mgr, 0x600);
    CHECK_EQ(Loaded(8), 1);
    // The same column again: nothing happens, nothing loads twice
    ObjectsManager_Update(&mgr, 0x610);
    CHECK_EQ(Loaded(8), 1);
}

// What the objects do themselves once they are out of range
static void CullAll(void) {
    for (int i = 0; i < LEVEL_OBJECTS; i++)
        ObjectDelete(&level_objects[i]);
}

static void ObjectsManager_BringsBackWhatIsBehind(void) {
    Reset(0x600);
    CHECK_EQ(Loaded(3), 0); // behind the start window: not loaded
    CHECK_EQ(Loaded(6), 1);
    CullAll();
    ObjectsManager_Update(&mgr, 0x300); // the camera moves back: the objects now in range on the left (0x280 and up) load
    CHECK_EQ(Loaded(2), 0);
    CHECK_EQ(Loaded(3), 1);
    CHECK_EQ(Loaded(4), 1);
    CHECK_EQ(Loaded(5), 1);
}

static Object *Find(uint8_t id) {
    Object *o = NULL;
    for (int i = 0; i < LEVEL_OBJECTS; i++)
        if (level_objects[i].type == id)
            o = &level_objects[i];
    return o;
}

static void ObjectsManager_RemembersState(void) {
    Reset(0x300);
    Object *o = Find(4); // the first object that remembers its state
    CHECK(o != NULL);
    if (o)
        CHECK_EQ(o->respawn_index, 1);
    CullAll(); // it dies for good (no Forget)
    ObjectsManager_Update(&mgr, 0x700);
    CullAll();
    ObjectsManager_Update(&mgr, 0x300); // and the camera comes back: 3 and 5 return, 4 does not
    CHECK_EQ(Loaded(3), 1);
    CHECK_EQ(Loaded(5), 1);
    CHECK_EQ(Loaded(4), 0);
}

static void ObjectsManager_ForgetLetsItComeBack(void) {
    Reset(0x300);
    Object *o = Find(4);
    CHECK(o != NULL);
    if (o)
        ObjectsManager_Forget(&mgr, o);
    CullAll();
    ObjectsManager_Update(&mgr, 0x700);
    CullAll();
    ObjectsManager_Update(&mgr, 0x300);
    CHECK_EQ(Loaded(4), 1); // forgotten, so it is loaded again
}

// The window is the game's to set: a narrower one loads less far ahead
static void ObjectsManager_WindowIsAParameter(void) {
    static const ObjectsManagerConfig narrow = { 0x180, 0x80 };
    memset(objects, 0, sizeof(objects));
    ObjectsManager_Init(&mgr, &narrow, marks, sizeof(marks), layout, 0);
    CHECK_EQ(Loaded(1), 1); // 0x100 < 0x180
    CHECK_EQ(Loaded(2), 0); // 0x200 is not
    ObjectsManager_Update(&mgr, 0x100);
    CHECK_EQ(Loaded(2), 1);
    CHECK_EQ(Loaded(3), 0); // 0x300 is not below 0x100 + 0x180
}

// The manager is plain data: a copy of it carries on exactly where the original was
static void ObjectsManager_IsAssignableData(void) {
    Reset(0);
    ObjectsManager copy = mgr;
    ObjectsManager_Update(&copy, 0x200);
    CHECK_EQ(Loaded(4), 1);
    CHECK_EQ(mgr.camera_x_last, 0); // the original did not move
    CHECK_EQ(copy.camera_x_last, 0x200);
}

// Sonic 2's PlaySoundLocal: a sound made by an object that is not on screen is not played
// ---- Two cameras (two-player mode) ----

static ObjectsManager2P mgr2;

static void Reset2P(int16_t camera_x, int16_t camera_x_p2) {
    static const ObjectsManagerConfig config = OBJECTS_MANAGER_DEFAULT_CONFIG;
    memset(objects, 0, sizeof(objects));
    ObjectsManager2P_Init(&mgr2, &config, marks, sizeof(marks), layout, camera_x, camera_x_p2);
}

static int Duplicates(void) { // objects with an id loaded more than once
    int n = 0;
    for (int id = 1; id <= 12; id++)
        if (Loaded((uint8_t)id) > 1)
            n++;
    return n;
}

// What the objects do themselves (MarkObjGone): one that is out of range of every camera deletes itself. In two-player mode "every camera" matters: an
// object near only the second camera has to stay.
static void CullOutOfRange(void) {
    for (int i = 0; i < LEVEL_OBJECTS; i++) {
        Object *o = &level_objects[i];
        if (o->type == 0)
            continue;
        bool near = false;
        for (int v = 0; v < 2; v++)
            near |= (uint16_t)(((uint16_t)o->pos.l.x.f.u & 0xFF80) - (uint16_t)mgr2.view[v].camera_x_coarse) <= 0x280;
        if (!near)
            memset(o, 0, sizeof(*o));
    }
}

static void ObjectsManager2P_EachCameraLoadsItsOwnWindow(void) {
    Reset2P(0, 0x800);
    CHECK_EQ(Loaded(1), 1); // the first camera's window
    CHECK_EQ(Loaded(2), 1);
    CHECK_EQ(Loaded(3), 0);
    CHECK_EQ(Loaded(7), 0); // nothing between them
    CHECK_EQ(Loaded(8), 1); // the second camera's: from 0x780 to 0xA80
    CHECK_EQ(Loaded(9), 1);
    CHECK_EQ(Loaded(10), 1);
    CHECK_EQ(Loaded(11), 0);
}

static void ObjectsManager2P_AnObjectBothCamerasHaveIsLoadedOnce(void) {
    Reset2P(0x100, 0x180); // the same objects in range of both (0x100 and 0x200, and 0x300 comes in at 0x180 + 0x280)
    CHECK_EQ(Loaded(1), 1);
    CHECK_EQ(Loaded(2), 1);
    CHECK_EQ(Loaded(3), 1);
    CHECK_EQ(Duplicates(), 0);
}

static void ObjectsManager2P_NeverLoadsTwiceWhileTheCamerasMove(void) {
    Reset2P(0, 0x600);
    for (int16_t x = 0; x <= 0xA00; x += 0x40) { // the first camera walks through the second's window and out the far side
        ObjectsManager2P_Update(&mgr2, x, 0x600);
        CullOutOfRange();
        CHECK_EQ(Duplicates(), 0);
    }
    for (int16_t x = 0xA00; x >= 0; x -= 0x40) { // and back again
        ObjectsManager2P_Update(&mgr2, x, 0x600);
        CullOutOfRange();
        CHECK_EQ(Duplicates(), 0);
    }
    for (int16_t x = 0x600; x >= 0; x -= 0x40) { // both together
        ObjectsManager2P_Update(&mgr2, x, x);
        CullOutOfRange();
        CHECK_EQ(Duplicates(), 0);
    }
}

static void ObjectsManager2P_TheSecondCamerasStartGapIsNotHeld(void) {
    Reset2P(0, 0x600);
    CHECK_EQ(Loaded(5), 0); // 0x500 is behind the second camera's start window: skipped, as with one camera
    ObjectsManager2P_Update(&mgr2, 0x300, 0x600); // ... but the first camera, coming up to it, must still load it
    CHECK_EQ(Loaded(5), 1);
    CHECK_EQ(Duplicates(), 0);
}

static void ObjectsManager2P_AnObjectIsThereWhileEitherCameraIsNear(void) {
    Reset2P(0, 0x800);
    ObjectsManager2P_Update(&mgr2, 0x800, 0x800); // the first camera has gone to the second's place; it holds the same objects
    CHECK_EQ(Loaded(8), 1);
    CHECK_EQ(Duplicates(), 0);
}

static void ObjectsManager2P_SharesTheMarks(void) {
    Reset2P(0x300, 0x900);
    // 0x400 remembers: the first camera's window loaded it, so the mark is set for the second's too
    CHECK(marks[1] & 0x80);
    Object *o = NULL;
    for (int i = 0; i < LEVEL_OBJECTS; i++)
        if (level_objects[i].type == 4)
            o = &level_objects[i];
    CHECK(o != NULL);
    if (o) {
        ObjectsManager2P_Forget(&mgr2, o);
        CHECK(!(marks[o->respawn_index] & 0x80));
    }
}

static void ObjectsManager2P_IsAssignableData(void) {
    Reset2P(0, 0x800);
    ObjectsManager2P copy = mgr2;
    ObjectsManager2P_Update(&copy, 0x200, 0x800); // the copy's views point at each other, not at the original's
    CHECK(copy.view[0].partner == &copy.view[1]);
    CHECK(copy.view[1].partner == &copy.view[0]);
    CHECK_EQ(Duplicates(), 0);
}

static void PlaySoundLocal_OnlyOnScreen(void) {
    extern SoundChipSet sound_sfx;
    Object o;
    memset(&o, 0, sizeof(o));
    uint8_t id = game_sound_bank.sfx_first;

    memset(sound_sfx.queue, 0, sizeof(sound_sfx.queue));
    o.render.f.on_screen = false;
    PlaySoundLocal(&o, id);
    CHECK_EQ(sound_sfx.queue[SOUND_QUEUE_SPECIAL], 0);

    o.render.f.on_screen = true;
    PlaySoundLocal(&o, id);
    CHECK_EQ(sound_sfx.queue[SOUND_QUEUE_SPECIAL], id);
    memset(sound_sfx.queue, 0, sizeof(sound_sfx.queue));
}

void RegisterObjectsManagerTests(void) {
    RUN_TEST(PlaySoundLocal_OnlyOnScreen);
    RUN_TEST(ObjectsManager2P_EachCameraLoadsItsOwnWindow);
    RUN_TEST(ObjectsManager2P_AnObjectBothCamerasHaveIsLoadedOnce);
    RUN_TEST(ObjectsManager2P_NeverLoadsTwiceWhileTheCamerasMove);
    RUN_TEST(ObjectsManager2P_TheSecondCamerasStartGapIsNotHeld);
    RUN_TEST(ObjectsManager2P_AnObjectIsThereWhileEitherCameraIsNear);
    RUN_TEST(ObjectsManager2P_SharesTheMarks);
    RUN_TEST(ObjectsManager2P_IsAssignableData);
    RUN_TEST(ObjectsManager_WindowIsAParameter);
    RUN_TEST(ObjectsManager_IsAssignableData);
    RUN_TEST(ObjectsManager_LoadsTheStartWindow);
    RUN_TEST(ObjectsManager_LoadsAheadWhenMovingRight);
    RUN_TEST(ObjectsManager_BringsBackWhatIsBehind);
    RUN_TEST(ObjectsManager_RemembersState);
    RUN_TEST(ObjectsManager_ForgetLetsItComeBack);
}
