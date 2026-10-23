#include "EditorModel.h"

#include <string.h>

#define OBJECT_ENTRY_BYTES 6
#define RING_ENTRY_BYTES   4
#define RING_STEP          0x18 // RingsManager.h's RING_SPACING

static uint16_t Be16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static void PutBe16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

void EditorLevel_Clear(EditorLevel *level) {
    memset(level, 0, sizeof(*level));
}

bool EditorLevel_LoadObjects(EditorLevel *level, const uint8_t *layout) {
    level->object_count = 0;
    if (layout == NULL)
        return true;
    for (const uint8_t *p = layout; Be16(p) != 0xFFFF; p += OBJECT_ENTRY_BYTES) {
        if (level->object_count >= EDITOR_MAX_OBJECTS)
            return false;
        const uint16_t y = Be16(p + 2);
        EditorObject *o = &level->objects[level->object_count++];
        o->x = Be16(p);
        o->y = y & 0x0FFF;
        o->x_flip = (y & 0x2000) != 0;
        o->y_flip = (y & 0x4000) != 0;
        o->remember = (y & 0x8000) != 0;
        o->spare = (y & 0x1000) != 0;
        o->id = p[4];
        o->subtype = p[5];
    }
    return true;
}

bool EditorLevel_LoadRings(EditorLevel *level, const uint8_t *layout) {
    level->ring_count = 0;
    if (layout == NULL)
        return true;
    for (const uint8_t *p = layout; Be16(p) != 0xFFFF; p += RING_ENTRY_BYTES) {
        if (level->ring_count >= EDITOR_MAX_RINGS)
            return false;
        const uint16_t y = Be16(p + 2);
        EditorRing *r = &level->rings[level->ring_count++];
        r->x = Be16(p);
        r->y = y & 0x0FFF;
        r->count = (uint8_t)(((y >> 12) & 7) + 1);
        r->vertical = (y & 0x8000) != 0;
    }
    return true;
}

// The index to insert an entry of X `x` at: after the last entry whose X is not greater
static int ObjectSlot(const EditorLevel *level, uint16_t x) {
    int i = level->object_count;
    while (i > 0 && level->objects[i - 1].x > x)
        i--;
    return i;
}

static int RingSlot(const EditorLevel *level, uint16_t x) {
    int i = level->ring_count;
    while (i > 0 && level->rings[i - 1].x > x)
        i--;
    return i;
}

int EditorLevel_AddObject(EditorLevel *level, const EditorObject *object) {
    if (level->object_count >= EDITOR_MAX_OBJECTS)
        return -1;
    const int at = ObjectSlot(level, object->x);
    memmove(&level->objects[at + 1], &level->objects[at], (size_t)(level->object_count - at) * sizeof(EditorObject));
    level->objects[at] = *object;
    level->objects[at].y &= 0x0FFF;
    level->object_count++;
    level->dirty = true;
    return at;
}

bool EditorLevel_RemoveObject(EditorLevel *level, int index) {
    if (index < 0 || index >= level->object_count)
        return false;
    memmove(&level->objects[index], &level->objects[index + 1], (size_t)(level->object_count - index - 1) * sizeof(EditorObject));
    level->object_count--;
    level->dirty = true;
    return true;
}

int EditorLevel_SetObject(EditorLevel *level, int index, const EditorObject *object) {
    if (index < 0 || index >= level->object_count)
        return -1;
    if (level->objects[index].x == object->x) { // (the same place in the order)
        level->objects[index] = *object;
        level->objects[index].y &= 0x0FFF;
        level->dirty = true;
        return index;
    }
    EditorLevel_RemoveObject(level, index);
    return EditorLevel_AddObject(level, object);
}

int EditorLevel_AddRing(EditorLevel *level, const EditorRing *ring) {
    if (level->ring_count >= EDITOR_MAX_RINGS)
        return -1;
    const int at = RingSlot(level, ring->x);
    memmove(&level->rings[at + 1], &level->rings[at], (size_t)(level->ring_count - at) * sizeof(EditorRing));
    level->rings[at] = *ring;
    level->rings[at].y &= 0x0FFF;
    if (level->rings[at].count < 1)
        level->rings[at].count = 1;
    if (level->rings[at].count > 8)
        level->rings[at].count = 8;
    level->ring_count++;
    level->dirty = true;
    return at;
}

bool EditorLevel_RemoveRing(EditorLevel *level, int index) {
    if (index < 0 || index >= level->ring_count)
        return false;
    memmove(&level->rings[index], &level->rings[index + 1], (size_t)(level->ring_count - index - 1) * sizeof(EditorRing));
    level->ring_count--;
    level->dirty = true;
    return true;
}

int EditorLevel_SetRing(EditorLevel *level, int index, const EditorRing *ring) {
    if (index < 0 || index >= level->ring_count)
        return -1;
    EditorLevel_RemoveRing(level, index);
    return EditorLevel_AddRing(level, ring);
}

int EditorLevel_FindObject(const EditorLevel *level, int x, int y, int radius) {
    int best = -1;
    long best_d = (long)radius * radius + 1;
    for (int i = 0; i < level->object_count; i++) {
        const long dx = level->objects[i].x - x, dy = level->objects[i].y - y;
        const long d = dx * dx + dy * dy;
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

int EditorLevel_FindRing(const EditorLevel *level, int x, int y, int radius) {
    int best = -1;
    long best_d = (long)radius * radius + 1;
    for (int i = 0; i < level->ring_count; i++) {
        const EditorRing *r = &level->rings[i];
        for (int n = 0; n < r->count; n++) {
            const long dx = (r->vertical ? r->x : r->x + n * RING_STEP) - x, dy = (r->vertical ? r->y + n * RING_STEP : r->y) - y;
            const long d = dx * dx + dy * dy;
            if (d < best_d) {
                best_d = d;
                best = i;
            }
        }
    }
    return best;
}

int EditorLevel_RememberCount(const EditorLevel *level) {
    int n = 0;
    for (int i = 0; i < level->object_count; i++)
        n += level->objects[i].remember;
    return n;
}

size_t EditorLevel_WriteObjects(const EditorLevel *level, uint8_t *out, size_t capacity) {
    const size_t need = (size_t)level->object_count * OBJECT_ENTRY_BYTES + OBJECT_ENTRY_BYTES; // (the end mark is as long as an entry: FFFF, 0, 0)
    if (capacity < need)
        return 0;
    uint8_t *p = out;
    for (int i = 0; i < level->object_count; i++, p += OBJECT_ENTRY_BYTES) {
        const EditorObject *o = &level->objects[i];
        PutBe16(p, o->x);
        PutBe16(p + 2, (uint16_t)((o->y & 0x0FFF) | (o->x_flip ? 0x2000 : 0) | (o->y_flip ? 0x4000 : 0) | (o->remember ? 0x8000 : 0) | (o->spare ? 0x1000 : 0)));
        p[4] = o->id;
        p[5] = o->subtype;
    }
    PutBe16(p, 0xFFFF);
    memset(p + 2, 0, OBJECT_ENTRY_BYTES - 2);
    return need;
}

size_t EditorLevel_WriteRings(const EditorLevel *level, uint8_t *out, size_t capacity) {
    const size_t need = (size_t)level->ring_count * RING_ENTRY_BYTES + RING_ENTRY_BYTES;
    if (capacity < need)
        return 0;
    uint8_t *p = out;
    for (int i = 0; i < level->ring_count; i++, p += RING_ENTRY_BYTES) {
        const EditorRing *r = &level->rings[i];
        PutBe16(p, r->x);
        PutBe16(p + 2, (uint16_t)((r->y & 0x0FFF) | ((uint16_t)(r->count - 1) << 12) | (r->vertical ? 0x8000 : 0)));
    }
    PutBe16(p, 0xFFFF);
    PutBe16(p + 2, 0);
    return need;
}
