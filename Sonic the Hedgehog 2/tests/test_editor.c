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

static void Editor_RingLinesAreWrittenAsOneEntryForEachRing(void) {
    EditorLevel_Clear(&level);
    const EditorRing r = { 0x80, 0x120, 5, true };
    EditorLevel_AddRing(&level, &r);
    const EditorRing h = { 0x80, 0x40, 3, false };
    EditorLevel_AddRing(&level, &h);
    uint8_t out[9 * 4];
    CHECK_EQ(EditorLevel_WriteRings(&level, out, sizeof(out)), 9 * 4); // 8 rings and the end mark
    // in X order, rings of the same X in the order they were added: the line down (x $80, y $120 on), then the row's first ring (x $80, y $40) comes after the column's first, its others at $98 and $B0
    CHECK_EQ(out[0] << 8 | out[1], 0x80);
    CHECK_EQ(out[2] << 8 | out[3], 0x120);
    CHECK_EQ(out[4] << 8 | out[5], 0x80);
    CHECK_EQ(out[6] << 8 | out[7], 0x138);
    CHECK_EQ(out[20] << 8 | out[21], 0x80); // the row's first ring
    CHECK_EQ(out[22] << 8 | out[23], 0x40);
    CHECK_EQ(out[24] << 8 | out[25], 0x98);
    CHECK_EQ(out[28] << 8 | out[29], 0xB0);
    CHECK_EQ(out[32] << 8 | out[33], 0xFFFF);
    CHECK_EQ(EditorLevel_FindRing(&level, 0x80, 0x120 + 3 * 0x18 + 2, 6), 0); // (the fourth ring of the line)
    EditorLevel_LoadRings(&level, out);
    CHECK_EQ(level.ring_count, 8); // the layout has them one by one
    CHECK_EQ(level.rings[0].count, 1);
}

static void Editor_NoMoreRingsThanTheStatusTableHolds(void) {
    EditorLevel_Clear(&level);
    EditorRing r = { 0, 0x40, 8, false };
    for (int i = 0; i < 63; i++) { // 63 lines of 8 are 504 rings
        r.x = (uint16_t)(i * 0xE0);
        CHECK(EditorLevel_AddRing(&level, &r) >= 0);
    }
    r.count = 7;
    CHECK(EditorLevel_AddRing(&level, &r) >= 0); // 511
    r.count = 1;
    CHECK_EQ(EditorLevel_AddRing(&level, &r), -1);
    r.count = 8;
    r.x = 0;
    CHECK_EQ(EditorLevel_SetRing(&level, 0, &r), 0); // the same size: fine
    CHECK_EQ(level.ring_count, 64);
}

void RegisterEditorTests(void) {
    RUN_TEST(Editor_EveryZonesObjectsAndRingsWriteBackAsTheyWereRead);
    RUN_TEST(Editor_AnObjectGoesInXOrderAndWritesItsFlags);
    RUN_TEST(Editor_MovingAnObjectPastAnotherReordersAndRemovingShrinks);
    RUN_TEST(Editor_RingLinesAreWrittenAsOneEntryForEachRing);
    RUN_TEST(Editor_NoMoreRingsThanTheStatusTableHolds);
}
