#include "test.h"

#include <string.h>

#include "EditorModel.h"
#include "Level.h"

// The built-in level editor's model (EditorModel.h): every object and ring layout of every zone must read in and write out as the same bytes, so a level saved without a change is the file it was.

static size_t LayoutBytes(const uint8_t *layout, size_t entry) {
    size_t n = 0;
    while (layout[n] != 0xFF || layout[n + 1] != 0xFF)
        n += entry;
    return n + (entry == 6 ? 6 : 2); // (the end mark: the objects' is a whole entry, the rings' may be just its X)
}

static EditorLevel level;

static void Editor_EveryZonesObjectsAndRingsWriteBackAsTheyWereRead(void) {
    int checked = 0;
    for (int zone = 0; zone < ZoneId_Num; zone++) {
        for (int act = 0; act < 4; act++) {
            const uint8_t *objects = level_obj[zone][act];
            const uint8_t *rings = level_ring[zone][act];
            EditorLevel_Clear(&level);
            if (objects != NULL && !(objects[0] == 0xFF && objects[1] == 0xFF && objects[2] == 0 && objects[3] == 0 && objects[4] == 0 && objects[5] == 0 && 0)) {
                CHECK(EditorLevel_LoadObjects(&level, objects));
                static uint8_t out[EDITOR_MAX_OBJECTS * 6 + 6];
                const size_t n = EditorLevel_WriteObjects(&level, out, sizeof(out));
                const size_t original = LayoutBytes(objects, 6) - 6; // (the entries; the end mark is checked below)
                CHECK_EQ(n, original + 6);
                CHECK(memcmp(out, objects, original) == 0);
                CHECK_EQ(out[original], 0xFF);
                CHECK_EQ(out[original + 1], 0xFF);
                checked++;
            }
            if (rings != NULL) {
                CHECK(EditorLevel_LoadRings(&level, rings));
                static uint8_t out[EDITOR_MAX_RINGS * 4 + 4];
                const size_t n = EditorLevel_WriteRings(&level, out, sizeof(out));
                const size_t original = LayoutBytes(rings, 4) - 2;
                CHECK(n >= original + 2);
                CHECK(memcmp(out, rings, original) == 0);
                checked++;
            }
        }
    }
    CHECK(checked > 20);
}

static void Editor_AnObjectGoesInXOrderAndWritesItsFlags(void) {
    EditorLevel_Clear(&level);
    const EditorObject a = { 0x200, 0x300, 0x26, 0x01, false, false, false, false };
    const EditorObject b = { 0x100, 0x7FF, 0x36, 0x00, true, true, true, false };
    CHECK_EQ(EditorLevel_AddObject(&level, &a), 0);
    CHECK_EQ(EditorLevel_AddObject(&level, &b), 0);   // (before the first)
    CHECK_EQ(level.object_count, 2);
    CHECK(level.dirty);
    uint8_t out[6 * 3];
    CHECK_EQ(EditorLevel_WriteObjects(&level, out, sizeof(out)), 18);
    static const uint8_t expected[18] = { 0x01, 0x00, 0xE7, 0xFF, 0x36, 0x00,  0x02, 0x00, 0x03, 0x00, 0x26, 0x01,  0xFF, 0xFF, 0, 0, 0, 0 };
    CHECK(memcmp(out, expected, 18) == 0);
    CHECK_EQ(EditorLevel_WriteObjects(&level, out, 17), 0); // (too small: nothing written)
}

static void Editor_MovingAnObjectPastAnotherReordersAndRemovingShrinks(void) {
    EditorLevel_Clear(&level);
    EditorObject o = { 0x100, 0x100, 1, 0, false, false, false, false };
    EditorLevel_AddObject(&level, &o);
    o.x = 0x200;
    EditorLevel_AddObject(&level, &o);
    EditorObject moved = level.objects[0];
    moved.x = 0x300;
    CHECK_EQ(EditorLevel_SetObject(&level, 0, &moved), 1);
    CHECK_EQ(level.objects[0].x, 0x200);
    CHECK_EQ(level.objects[1].x, 0x300);
    CHECK_EQ(EditorLevel_FindObject(&level, 0x2FA, 0x100, 16), 1);
    CHECK_EQ(EditorLevel_FindObject(&level, 0x900, 0x100, 16), -1);
    CHECK(EditorLevel_RemoveObject(&level, 0));
    CHECK(!EditorLevel_RemoveObject(&level, 5));
    CHECK_EQ(level.object_count, 1);
}

static void Editor_RingLinesKeepTheirCountAndDirection(void) {
    EditorLevel_Clear(&level);
    const EditorRing r = { 0x80, 0x120, 5, true };
    EditorLevel_AddRing(&level, &r);
    uint8_t out[8];
    CHECK_EQ(EditorLevel_WriteRings(&level, out, sizeof(out)), 8);
    CHECK_EQ(out[2], 0xC1); // vertical, 5 rings (count - 1 = 4), y $120: $8000 | $4000 | $120
    CHECK_EQ(out[3], 0x20);
    CHECK_EQ(EditorLevel_FindRing(&level, 0x80, 0x120 + 3 * 0x18 + 2, 6), 0); // (the fourth ring of the line)
    EditorLevel_LoadRings(&level, out);
    CHECK_EQ(level.rings[0].count, 5);
    CHECK(level.rings[0].vertical);
}

void RegisterEditorTests(void) {
    RUN_TEST(Editor_EveryZonesObjectsAndRingsWriteBackAsTheyWereRead);
    RUN_TEST(Editor_AnObjectGoesInXOrderAndWritesItsFlags);
    RUN_TEST(Editor_MovingAnObjectPastAnotherReordersAndRemovingShrinks);
    RUN_TEST(Editor_RingLinesKeepTheirCountAndDirection);
}
