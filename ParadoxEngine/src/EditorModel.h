#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// The built-in level editor's model of a level's objects and rings: plain data the editor changes and writes out in the engine's own layout formats (ObjectsManager.h and RingsManager.h), the files a game keeps under
// res/. Nothing here knows a game, a file path or a user interface: the Qt tools and the game's hooks work on it.
//
// Entries are kept in layout order (sorted by X, entries with the same X in the order they were added), which is what the managers need, so what is written out loads back unchanged.

#ifdef __cplusplus
extern "C" {
#endif

#define EDITOR_MAX_OBJECTS 0x400
#define EDITOR_MAX_RINGS   0x400

typedef struct {
    uint16_t x;
    uint16_t y;       // 0-$FFF
    uint8_t id;
    uint8_t subtype;
    bool x_flip;
    bool y_flip;
    bool remember;    // the object remembers its state once loaded (it gets a respawn index)
    bool spare;       // bit 12 of the Y word, which the engine does not read (the layouts of some zones have it set): kept as it was
} EditorObject;

typedef struct {
    uint16_t x;
    uint16_t y;       // 0-$FFF
    uint8_t count;    // 1-8 rings in a line
    bool vertical;    // the line goes down, not right
} EditorRing;

typedef struct {
    EditorObject objects[EDITOR_MAX_OBJECTS];
    int object_count;
    EditorRing rings[EDITOR_MAX_RINGS];
    int ring_count;
    bool dirty;       // changed since it was loaded or last marked saved
} EditorLevel;

void EditorLevel_Clear(EditorLevel *level);

// Reads a layout in the engine's format (NULL or one that starts with the end mark is an empty list). False if the list has more entries than the model holds (what fits is kept).
bool EditorLevel_LoadObjects(EditorLevel *level, const uint8_t *layout);
bool EditorLevel_LoadRings(EditorLevel *level, const uint8_t *layout);

// Adding puts the entry in X order and returns its index (-1: the list is full). Setting replaces an entry and returns its new index, which changes when its X does. Removing returns false for an index out of range.
int EditorLevel_AddObject(EditorLevel *level, const EditorObject *object);
int EditorLevel_SetObject(EditorLevel *level, int index, const EditorObject *object);
bool EditorLevel_RemoveObject(EditorLevel *level, int index);
int EditorLevel_AddRing(EditorLevel *level, const EditorRing *ring);
int EditorLevel_SetRing(EditorLevel *level, int index, const EditorRing *ring);
bool EditorLevel_RemoveRing(EditorLevel *level, int index);

// The entry nearest to a point within `radius` pixels (by its own position; a ring entry by any of its rings), or -1
int EditorLevel_FindObject(const EditorLevel *level, int x, int y, int radius);
int EditorLevel_FindRing(const EditorLevel *level, int x, int y, int radius);

// How many objects remember their state: the game's table of marks has a limit (Sonic 1 and 2: 255), past which the extra ones are not remembered
int EditorLevel_RememberCount(const EditorLevel *level);

// Writes the list in the engine's layout format, with its end mark; returns the bytes it needs (0 when `capacity` is too small, nothing written)
size_t EditorLevel_WriteObjects(const EditorLevel *level, uint8_t *out, size_t capacity);
size_t EditorLevel_WriteRings(const EditorLevel *level, uint8_t *out, size_t capacity);

#ifdef __cplusplus
}
#endif
